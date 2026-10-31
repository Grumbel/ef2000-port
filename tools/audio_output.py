# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Capture original DMA PCM and optionally send it to SDL's queued audio API."""

from array import array
import ctypes as c
from ctypes.util import find_library
import hashlib
import json
import os
import sys
import wave


class AudioSpec(c.Structure):
    _fields_ = [('freq', c.c_int), ('format', c.c_uint16), ('channels', c.c_uint8),
                ('silence', c.c_uint8), ('samples', c.c_uint16), ('padding', c.c_uint16),
                ('size', c.c_uint32), ('callback', c.c_void_p), ('userdata', c.c_void_p)]


class AudioPlayback:
    def __init__(self):
        library = os.environ.get('EF2000_SDL_LIBRARY') or find_library('SDL2')
        if not library:
            raise ValueError('SDL2 audio unavailable')
        self.sdl = c.CDLL(library)
        signatures = {
            'SDL_InitSubSystem': ([c.c_uint32], c.c_int),
            'SDL_QuitSubSystem': ([c.c_uint32], None),
            'SDL_GetError': ([], c.c_char_p),
            'SDL_OpenAudioDevice': ([c.c_char_p, c.c_int, c.POINTER(AudioSpec), c.POINTER(AudioSpec), c.c_int], c.c_uint32),
            'SDL_CloseAudioDevice': ([c.c_uint32], None),
            'SDL_PauseAudioDevice': ([c.c_uint32, c.c_int], None),
            'SDL_QueueAudio': ([c.c_uint32, c.c_void_p, c.c_uint32], c.c_int),
            'SDL_GetQueuedAudioSize': ([c.c_uint32], c.c_uint32),
            'SDL_ClearQueuedAudio': ([c.c_uint32], None),
            'SDL_NewAudioStream': ([c.c_uint16, c.c_uint8, c.c_int, c.c_uint16, c.c_uint8, c.c_int], c.c_void_p),
            'SDL_AudioStreamPut': ([c.c_void_p, c.c_void_p, c.c_int], c.c_int),
            'SDL_AudioStreamAvailable': ([c.c_void_p], c.c_int),
            'SDL_AudioStreamGet': ([c.c_void_p, c.c_void_p, c.c_int], c.c_int),
            'SDL_FreeAudioStream': ([c.c_void_p], None),
        }
        for name, (args, result) in signatures.items():
            getattr(self.sdl, name).argtypes = args
            getattr(self.sdl, name).restype = result
        self.stream = None
        self.signature = None
        self.queued_bytes = 0
        self.last_peak = 0
        self.queue_resets = 0
        if self.sdl.SDL_InitSubSystem(0x10) < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        desired = AudioSpec(freq=44100, format=0x8010, channels=2, samples=1024)
        obtained = AudioSpec()
        self.device = self.sdl.SDL_OpenAudioDevice(None, 0, c.byref(desired), c.byref(obtained), 0)
        if not self.device:
            self.sdl.SDL_QuitSubSystem(0x10)
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.sdl.SDL_PauseAudioDevice(self.device, 0)

    def push(self, pcm, bits, channels, rate, generation, gains=(1, 1)):
        signature = bits, channels, rate, generation
        if signature != self.signature:
            if self.stream:
                self.sdl.SDL_FreeAudioStream(self.stream)
            self.stream = self.sdl.SDL_NewAudioStream(0x8010 if bits == 16 else 8, channels, rate,
                                                    0x8010, 2, 44100)
            if not self.stream:
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.signature = signature
        if self.sdl.SDL_AudioStreamPut(self.stream, c.c_char_p(pcm), len(pcm)) < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        available = self.sdl.SDL_AudioStreamAvailable(self.stream)
        if available < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        if available:
            data = c.create_string_buffer(available)
            count = self.sdl.SDL_AudioStreamGet(self.stream, data, available)
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
            if self.sdl.SDL_GetQueuedAudioSize(self.device) > 44100 * 4 * 2:
                self.sdl.SDL_ClearQueuedAudio(self.device)
                self.queue_resets += 1
            if self.sdl.SDL_QueueAudio(self.device, c.c_char_p(pcm_output), count) < 0:
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.queued_bytes += count

    def close(self):
        if self.stream:
            self.sdl.SDL_FreeAudioStream(self.stream)
            self.stream = None
        self.sdl.SDL_CloseAudioDevice(self.device)
        self.sdl.SDL_QuitSubSystem(0x10)


class AudioCapture:
    def __init__(self, output, playback=False):
        self.output = output
        self.playback = AudioPlayback() if playback else None
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
