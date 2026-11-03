# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free contract tests for the OpenWatcom compile wrapper."""

from pathlib import Path
import tempfile
import unittest

from tools import ef2000_wcc


class WatcomWrapperTests(unittest.TestCase):
    def test_compiles_fixture_with_pinned_flags(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'smoke.obj'
            def fake_compiler(argv, check):
                self.assertTrue(check)
                self.assertEqual(argv[0], '/example/wcc386')
                for flag in ef2000_wcc.FLAGS:
                    self.assertIn(flag, argv)
                self.assertIn('-s', argv)  # No unresolved __CHK dependency.
                self.assertIn('-fo=' + str(output), argv)
                output.write_bytes(b'fictional OMF fixture')
            result = ef2000_wcc.compile_module(
                ef2000_wcc.ROOT / 'decomp/tests/fixture.c',
                compiler='/example/wcc386', output=output, execute=fake_compiler)
            self.assertEqual(result, output)

    def test_rejects_non_decomp_source(self):
        with self.assertRaisesRegex(ValueError, 'under decomp'):
            ef2000_wcc.compile_module(ef2000_wcc.ROOT / 'runtime/startup.c')

    def test_missing_object_is_a_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'missing.obj'
            with self.assertRaisesRegex(ValueError, 'without creating'):
                ef2000_wcc.compile_module(
                    ef2000_wcc.ROOT / 'decomp/tests/fixture.c',
                    output=output, execute=lambda argv, check: None)


if __name__ == '__main__':
    unittest.main()
