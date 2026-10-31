# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
import wave

from tools.audio_output import AudioCapture, AudioPlayback


class AudioCaptureTests(unittest.TestCase):
    def test_unsigned_word_pcm_converts_to_signed_wav_and_keeps_frame_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            capture = AudioCapture(output)
            block = dict(bits=16, channels=1, signed=False, rate=22050, generation=1)
            capture.push(block | {'pcm': b'\x00'})
            capture.push(block | {'pcm': b'\x00\x00\x80\xff\xff'})
            capture.close(3)
            report = json.loads((output / 'audio.json').read_text())
            self.assertEqual(report['interrupts'], 3)
            self.assertEqual(report['segments'][0]['frames'], 3)
            with wave.open(str(output / 'audio-000.wav'), 'rb') as stream:
                self.assertEqual(stream.getparams()[:3], (1, 2, 22050))
                self.assertEqual(struct.unpack('<3h', stream.readframes(3)), (-32768, 0, 32767))

    def test_reset_discards_incomplete_self_test_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            capture = AudioCapture(output)
            block = dict(bits=16, channels=2, signed=True, rate=22050, generation=1)
            capture.push(block | {'pcm': b'\x80\x80'})
            capture.push(block | {'generation': 2, 'pcm': b'\0\0\0\0'})
            capture.close()
            report = json.loads((output / 'audio.json').read_text())
            self.assertEqual(report['discarded_partial_bytes'], 2)
            self.assertEqual(len(report['segments']), 1)
            self.assertEqual(report['segments'][0]['frames'], 1)
            self.assertEqual(report['segments'][0]['peak'], 0)


@unittest.skipUnless(os.environ.get('EF2000_TEST_AUDIO') == 'dummy', 'requires opt-in dummy SDL audio')
class SDLPlaybackTests(unittest.TestCase):
    def test_pcm_is_resampled_and_queued_to_real_sdl_device(self):
        self.assertEqual(os.environ.get('SDL_AUDIODRIVER'), 'dummy')
        output = AudioPlayback()
        try:
            pcm = struct.pack('<h', 1234) * 2205
            output.push(pcm, 16, 1, 22050, 1)
            self.assertGreater(output.queued_bytes, 10000)
            self.assertEqual(output.queue_resets, 0)
            self.assertGreater(output.last_peak, 0)
            output.push(pcm, 16, 1, 22050, 1, gains=(0, 0))
            self.assertEqual(output.last_peak, 0)
            output.push(bytes([128]) * 2205, 8, 1, 22050, 2)
            self.assertGreater(output.queued_bytes, 20000)
        finally:
            output.close()

class AudioMixerTests(unittest.TestCase):
    def test_mixes_sources_on_one_clock_and_clips(self):
        from tools.audio_output import AudioMixer
        from unittest.mock import Mock, patch
        mixer = AudioMixer.__new__(AudioMixer)
        mixer.output = Mock()
        mixer.next_frame = 0
        mixer.buffers = {1: bytearray(struct.pack('<4h', 30000, -30000, 100, 200)),
                         2: bytearray(struct.pack('<2h', 10000, -10000))}
        with patch('tools.audio_output.time.monotonic', return_value=1/44100 + 1e-10):
            mixer.flush()
        self.assertEqual(mixer.output.push.call_args.args[0], struct.pack('<2h', 32767, -32768))
        self.assertEqual(mixer.buffers[1], struct.pack('<2h', 100, 200))
        self.assertEqual(mixer.buffers[2], b'')
