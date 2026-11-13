# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools import asm_inventory


class InventoryTests(unittest.TestCase):
    def fixture(self, root):
        image = root / 'payload.bin'
        image.write_bytes(bytes.fromhex('e80100000090c3'))
        rows = [dict(addr='0x0', size='7', name='fixture', state='drafted',
                     kind='game', module='fixture'),
                dict(addr='0x6', size='1', name='tail', state='unknown',
                     kind='asm', module='')]
        return image, rows

    def test_real_decoder_bytes_targets_overlap_and_reproducibility(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            first = asm_inventory.export_inventory(image, rows, root / 'a')
            second = asm_inventory.export_inventory(image, rows, root / 'b')
            self.assertEqual(first, second)
            for path in (root / 'a').iterdir():
                self.assertEqual(path.read_bytes(), (root / 'b' / path.name).read_bytes())
            record = json.loads((root / 'a/00000000.json').read_text())
            self.assertEqual(record['overlapping_entries'], [6])
            self.assertEqual(record['instructions'][0]['direct_target'], 6)
            self.assertEqual(record['instructions'][0]['bytes'], 'e801000000')
            self.assertEqual(record['boundary'], 'unverified-linear-window')
            with self.assertRaises(FileExistsError):
                asm_inventory.export_inventory(image, rows, root / 'a')

    def test_reject_wrong_decoder_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            def runner(argv, **kwargs):
                text = 'fake decoder' if '--version' in argv else '0: c3 ret'
                return subprocess.CompletedProcess(argv, 0, text, '')
            with self.assertRaisesRegex(ValueError, 'byte/continuity mismatch'):
                asm_inventory.export_inventory(image, rows, root / 'out', runner=runner)

    def test_validate_extents_before_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            rows[0]['size'] = '100'
            with self.assertRaisesRegex(ValueError, 'invalid linear window'):
                asm_inventory.export_inventory(image, rows, root / 'out')
            self.assertFalse((root / 'out').exists())


if __name__ == '__main__':
    unittest.main()
