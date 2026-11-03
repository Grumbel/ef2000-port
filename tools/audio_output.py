# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Capture original DMA PCM and optionally send it to SDL's queued audio API."""

from array import array
import ctypes as c
from ctypes.util import find_library
import hashlib
import json
import os
import sys
import time
import wave


class AudioSpec(c.Structure):
    _fields_ = [('format', c.c_uint32), ('channels', c.c_int), ('freq', c.c_int)]


class AudioPlayback:
    def __init__(self, mixer=None):
        self.mixer = mixer
        if mixer is None:
            library = os.environ.get('EF2000_SDL_LIBRARY') or find_library('SDL3')
            if not library:
                raise ValueError('SDL3 audio unavailable')
            self.sdl = c.CDLL(library)
            signatures = {
                'SDL_InitSubSystem': ([c.c_uint32], c.c_bool),
                'SDL_QuitSubSystem': ([c.c_uint32], None),
                'SDL_GetError': ([], c.c_char_p),
                'SDL_OpenAudioDeviceStream': ([c.c_uint32, c.POINTER(AudioSpec), c.c_void_p, c.c_void_p], c.c_void_p),
                'SDL_ResumeAudioStreamDevice': ([c.c_void_p], c.c_bool),
                'SDL_CreateAudioStream': ([c.POINTER(AudioSpec), c.POINTER(AudioSpec)], c.c_void_p),
                'SDL_PutAudioStreamData': ([c.c_void_p, c.c_void_p, c.c_int], c.c_bool),
                'SDL_GetAudioStreamAvailable': ([c.c_void_p], c.c_int),
                'SDL_GetAudioStreamQueued': ([c.c_void_p], c.c_int),
                'SDL_GetAudioStreamData': ([c.c_void_p, c.c_void_p, c.c_int], c.c_int),
                'SDL_ClearAudioStream': ([c.c_void_p], c.c_bool),
                'SDL_DestroyAudioStream': ([c.c_void_p], None),
            }
            for name, (args, result) in signatures.items():
                getattr(self.sdl, name).argtypes = args
                getattr(self.sdl, name).restype = result
        else:
            self.sdl = mixer.output.sdl
        self.stream = None
        self.signature = None
        self.queued_bytes = 0
        self.last_peak = 0
        self.queue_resets = 0
        if mixer is not None:
            mixer.buffers[self] = bytearray()
            return
        if not self.sdl.SDL_InitSubSystem(0x10):
            raise ValueError(self.sdl.SDL_GetError().decode())
        desired = AudioSpec(format=0x8010, channels=2, freq=44100)
        self.device_stream = self.sdl.SDL_OpenAudioDeviceStream(0xffffffff, c.byref(desired), None, None)
        if not self.device_stream:
            self.sdl.SDL_QuitSubSystem(0x10)
            raise ValueError(self.sdl.SDL_GetError().decode())
        if not self.sdl.SDL_ResumeAudioStreamDevice(self.device_stream):
            self.sdl.SDL_DestroyAudioStream(self.device_stream)
            self.sdl.SDL_QuitSubSystem(0x10)
            raise ValueError(self.sdl.SDL_GetError().decode())

    def push(self, pcm, bits, channels, rate, generation, gains=(1, 1)):
        signature = bits, channels, rate, generation
        if signature != self.signature:
            if self.stream:
                self.sdl.SDL_DestroyAudioStream(self.stream)
            source = AudioSpec(format=0x8010 if bits == 16 else 8, channels=channels, freq=rate)
            target = AudioSpec(format=0x8010, channels=2, freq=44100)
            self.stream = self.sdl.SDL_CreateAudioStream(c.byref(source), c.byref(target))
            if not self.stream:
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.signature = signature
        if not self.sdl.SDL_PutAudioStreamData(self.stream, c.c_char_p(pcm), len(pcm)):
            raise ValueError(self.sdl.SDL_GetError().decode())
        available = self.sdl.SDL_GetAudioStreamAvailable(self.stream)
        if available < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        if available:
            data = c.create_string_buffer(available)
            count = self.sdl.SDL_GetAudioStreamData(self.stream, data, available)
            if count < 0:
                raise ValueError(self.sdl.SDL_GetError().decode())
            converted = array('h', data.raw[:count])
            if sys.byteorder != 'little':
                converted.byteswap()
            if gains != (1, 1):
                for index, sample in enumerate(converted):
                    converted[index] = int(sample * gains[index & 1])
            self.last_peak = max((abs(v) for v in converted), default=0)
            if sys.byteorder != 'little':
                converted.byteswap()
            pcm_output = converted.tobytes()
            if self.mixer is not None:
                self.mixer.buffers[self].extend(pcm_output)
                self.queued_bytes += count
                return
            if self.sdl.SDL_GetAudioStreamQueued(self.device_stream) > 44100 * 4 * 2:
                if not self.sdl.SDL_ClearAudioStream(self.device_stream):
                    raise ValueError(self.sdl.SDL_GetError().decode())
                self.queue_resets += 1
            if not self.sdl.SDL_PutAudioStreamData(self.device_stream, c.c_char_p(pcm_output), count):
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.queued_bytes += count

    def close(self):
        if self.stream:
            self.sdl.SDL_DestroyAudioStream(self.stream)
            self.stream = None
        if self.mixer is None:
            self.sdl.SDL_DestroyAudioStream(self.device_stream)
            self.sdl.SDL_QuitSubSystem(0x10)


class AudioMixer:
    """Mix independently converted stereo sources into one SDL device."""
    def __init__(self):
        self.output = AudioPlayback()
        self.buffers = {}
        self.next_frame = time.monotonic() + 0.1

    def source(self):
        return AudioPlayback(self)

    def flush(self):
        now = time.monotonic()
        frames = int((now - self.next_frame) * 44100)
        if frames <= 0:
            return
        self.next_frame += frames / 44100
        size = frames * 4
        mixed = [0] * (size // 2)
        for data in self.buffers.values():
            samples = array('h', data[:size])
            if sys.byteorder != 'little':
                samples.byteswap()
            for i, sample in enumerate(samples):
                mixed[i] += sample
            del data[:size]
        pcm = array('h', (max(-32768, min(32767, v)) for v in mixed))
        if sys.byteorder != 'little':
            pcm.byteswap()
        self.output.push(pcm.tobytes(), 16, 2, 44100, 0)

    def close(self):
        self.output.close()


class AudioCapture:
    def __init__(self, output, playback=False):
        self.output = output
        self.playback = AudioPlayback() if playback is True else playback or None
        self.signature = None
        self.carry = b''
        self.writer = None
        self.segments = []
        self.discarded_partial_bytes = 0

    def push(self, block):
        signature = tuple(block[key] for key in ('bits', 'channels', 'signed', 'rate', 'generation'))
        if signature != self.signature:
            if self.writer:
                self.writer.close()
                self.writer = None
            self.discarded_partial_bytes += len(self.carry)
            self.carry = b''
            self.signature = signature
        bits, channels, signed, rate, generation = signature
        frame_size = (bits // 8) * channels
        data = self.carry + block['pcm']
        length = len(data) // frame_size * frame_size
        data, self.carry = data[:length], data[length:]
        if not data:
            return
        pcm = bytearray(data)
        if bits == 8 and signed:
            pcm[:] = bytes(v ^ 128 for v in pcm)
        elif bits == 16 and not signed:
            pcm[1::2] = bytes(v ^ 128 for v in pcm[1::2])
        pcm = bytes(pcm)
        if self.writer is None:
            name = f'audio-{len(self.segments):03d}.wav'
            self.writer = wave.open(str(self.output / name), 'wb')
            self.writer.setparams((channels, bits // 8, rate, 0, 'NONE', 'not compressed'))
            self.segments.append({'path': name, 'bits': bits, 'channels': channels, 'rate': rate,
                                  'generation': generation, 'frames': 0, 'nonzero_samples': 0, 'peak': 0})
        self.writer.writeframesraw(pcm)
        segment = self.segments[-1]
        segment['frames'] += len(pcm) // frame_size
        if bits == 16:
            values = array('h', pcm)
            if sys.byteorder != 'little':
                values.byteswap()
        else:
            values = [v - 128 for v in pcm]
        segment['nonzero_samples'] += sum(v != 0 for v in values)
        segment['peak'] = max(segment['peak'], max((abs(v) for v in values), default=0))
        if self.playback:
            self.playback.push(pcm, bits, channels, rate, generation, block.get('gains', (1, 1)))

    def close(self, interrupts=0):
        if self.writer:
            self.writer.close()
        self.discarded_partial_bytes += len(self.carry)
        for segment in self.segments:
            path = self.output / segment['path']
            segment['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        report = {'segments': self.segments, 'interrupts': interrupts,
                  'discarded_partial_bytes': self.discarded_partial_bytes,
                  'playback': bool(self.playback)}
        if self.playback:
            report['queued_bytes'] = self.playback.queued_bytes
            report['queue_resets'] = self.playback.queue_resets
            self.playback.close()
        (self.output / 'audio.json').write_text(json.dumps(report, indent=2) + '\n')
