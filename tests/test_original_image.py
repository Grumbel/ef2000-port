# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Opt-in checks against an external original; no game bytes stored here."""

import hashlib
import os
from pathlib import Path
import unittest

from tools.extract_image import extract, u16


@unittest.skipUnless(os.environ.get('EF2000_EXE'), 'set EF2000_EXE to test original image')
class OriginalImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = Path(os.environ['EF2000_EXE']).read_bytes()
        cls.manifest, cls.images = extract(cls.original)

    def test_both_loader_relocation_passes(self):
        self.assertEqual(u16(self.images['stage1-relocated.bin'], 8), 0x1095)
        self.assertEqual(u16(self.images['stage1-relocated.bin'], 0x12), 0x1010)
        self.assertEqual(u16(self.images['stage2-relocated.bin'], 0x100), 0x1505)
        self.assertEqual(self.manifest['real_mode_model']['stage2_relocations_applied'], 32)

    def test_payload_matches_recovered_startup(self):
        payload = self.images['payload.bin']
        self.assertEqual(len(payload), 0x195fb0)
        self.assertEqual(hashlib.sha256(payload).hexdigest(),
                         '3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb')
        self.assertEqual(payload[0x86:0x95],
                         bytes.fromhex('66 8c db 3e b9 60 02 00 00 66 b8 05 35 cd 21'))
        self.assertEqual(self.manifest['layout']['payload_file_offset'], 0x54a1)
        self.assertEqual(self.manifest['layout']['bss_end'], 0x4de2f0)

    def test_rejects_unusable_load_segment(self):
        for segment in (-1, 0xa000):
            with self.assertRaises(ValueError):
                extract(self.original, load_segment=segment)


if __name__ == '__main__':
    unittest.main()
