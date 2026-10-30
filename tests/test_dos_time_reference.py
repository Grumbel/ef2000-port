# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import json
from pathlib import Path
import unittest

from tools.read_dos_time_probe import decode


class DosTimeReferenceTests(unittest.TestCase):
    def test_measured_register_preservation_and_advancing_time(self):
        reference = json.loads((Path(__file__).resolve().parent.parent /
                                'docs/dos-time-reference.json').read_text())
        times = []
        for sample in reference['samples']:
            self.assertEqual(sample['eax'], 0x2c00)
            self.assertEqual(sample['ebx'], 0x11223344)
            self.assertEqual(sample['esi'], 0x12345678)
            self.assertEqual(sample['edi'], 0x89abcdef)
            self.assertEqual(sample['ecx'] >> 16, 0x5566)
            self.assertEqual(sample['edx'] >> 16, 0x99aa)
            self.assertEqual(sample['eflags'], sample['input_flags'])
            hour, minute = (sample['ecx'] >> 8) & 255, sample['ecx'] & 255
            second, hundredth = (sample['edx'] >> 8) & 255, sample['edx'] & 255
            self.assertLess(hour, 24)
            self.assertLess(minute, 60)
            self.assertLess(second, 60)
            self.assertLess(hundredth, 100)
            times.append(((hour * 60 + minute) * 60 + second) * 100 + hundredth)
        self.assertGreater((times[-1] - times[0]) % 8640000, 0)

    def test_rejects_incomplete_records(self):
        for data in (b'', bytes(0x94), b'TIME\x01\0\0\0'):
            with self.assertRaises(ValueError):
                decode(data)
