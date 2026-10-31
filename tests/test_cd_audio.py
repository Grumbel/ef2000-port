# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from tools.cd_audio import CDAudio


class CDAudioTest(unittest.TestCase):
    def test_guest_track_query_play_and_stop(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'disc.bin').write_bytes(bytes(2352) + b'\x01\x02' * (2352 * 2))
            (root / 'disc.cue').write_text('FILE "disc.bin" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 00:00:01\n')
            cd = CDAudio(root / 'disc.cue')
            self.addCleanup(cd.close)
            values = [0] * 14
            values[2], values[4], values[9] = 0x1510, 3, 0x1000
            memory = bytearray(0x100000)
            request, data = 0x10000, 0x20000
            memory[request+2] = 3
            struct.pack_into('<HHH', memory, request+14, 0, 0x2000, 7)
            memory[data:data+2] = bytes((11, 2))
            _, result = cd.interrupt(values, memory)
            self.assertEqual(result[data:data+7], b'\x0b\x02\x01\x02\x00\x00\x00')
            memory[request+2] = 0x84
            struct.pack_into('<II', memory, request+14, 1, 2)
            with patch('tools.cd_audio.time.monotonic', return_value=100):
                _, result = cd.interrupt(values, memory)
            self.assertEqual(struct.unpack_from('<H', result, request+3)[0], 0x300)
            with patch('tools.cd_audio.time.monotonic', return_value=100 + 1/75 + .00001):
                self.assertEqual(cd.pcm()['pcm'], b'\x01\x02' * 1176)
                memory[request+2] = 0x85
                cd.interrupt(values, memory)
                self.assertFalse(cd.playing())
            self.assertIsNone(cd.pcm())
