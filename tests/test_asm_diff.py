# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free command/output tests; actual wdis runs in Nix toolchain."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from tools import asm_diff


class AsmDiffTests(unittest.TestCase):
    def test_linear_guest_window_and_omf_relocations(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            image = root / 'payload.bin'
            image.touch()
            obj = root / 'fixture.obj'
            obj.touch()
            def runner(argv, **kwargs):
                if argv[0] == 'objdump':
                    self.assertIn('--start-address=0x4050', argv)
                    self.assertIn('--stop-address=0x4058', argv)
                    return subprocess.CompletedProcess(argv, 0,
                        '  4050: 53                    push ebx\n', '')
                self.assertEqual(argv[0], 'wdis')
                self.assertIn('-e', argv)
                self.assertIn('-p', argv)
                self.assertNotIn('-r', argv)
                Path(next(a[3:] for a in argv if a.startswith('-l='))).write_text(
                    'fixture_symbol: add eax,1 ; FIXUP external_symbol\n')
                return subprocess.CompletedProcess(argv, 0, '', '')
            left = asm_diff.original_listing(image, 0x4050, 8, runner=runner)
            right = asm_diff.omf_listing(obj, runner=runner)
            self.assertIn('push ebx', left[0])
            self.assertIn('FIXUP external_symbol', right[0])

    def test_out_of_range_address(self):
        with self.assertRaisesRegex(ValueError, 'outside initialized image'):
            asm_diff.original_listing(Path('/nonexistent'), 0x200000, 8)


if __name__ == '__main__':
    unittest.main()
