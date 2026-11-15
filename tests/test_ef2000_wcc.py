# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free contract tests for the OpenWatcom compile wrapper."""

from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import tempfile
import threading
import unittest
from unittest import mock

from tools import ef2000_wcc


class WatcomWrapperTests(unittest.TestCase):
    def test_concurrent_objects_keep_their_own_abi_headers(self):
        with tempfile.TemporaryDirectory() as temp:
            ready = threading.Barrier(2)
            job = threading.local()
            def fake_compiler(argv, check):
                header = Path(next(arg[4:] for arg in argv if arg.startswith('-fi=')))
                output = Path(next(arg[4:] for arg in argv if arg.startswith('-fo=')))
                ready.wait(timeout=5)  # Both headers exist before either is read.
                self.assertEqual(header.read_text(), job.pragmas)
                output.write_bytes(b'fictional OMF fixture')
            def compile_one(name):
                job.pragmas = '#pragma aux ' + name + ' parm [eax]\n'
                return ef2000_wcc.compile_module(
                    ef2000_wcc.ROOT / 'decomp/tests/fixture.c',
                    output=Path(temp) / (name + '.obj'), execute=fake_compiler)
            with mock.patch.object(ef2000_wcc, 'confirmed_abi_pragmas',
                                   side_effect=lambda: job.pragmas):
                with ThreadPoolExecutor(max_workers=2) as workers:
                    results = list(workers.map(compile_one, ('first', 'second')))
            self.assertEqual({p.name for p in results}, {'first.obj', 'second.obj'})

    def test_explicit_cdecl_is_not_overridden_by_register_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'decomp/symbols').mkdir(parents=True)
            (root / 'decomp/include').mkdir()
            (root / 'decomp/include/ef_abi.h').write_text(
                'uint32_t EF_CDECL guest_stack(uint32_t a); /* 0x100 */\n'
                'uint32_t __cdecl guest_stack_literal(uint32_t a); /* 0x100 */\n'
                'uint32_t guest_register(uint32_t a); /* 0x100 */\n')
            (root / 'decomp/symbols/signatures.tsv').write_text(
                'addr\tname\tinputs\toutputs\tpreserved\tstack_bytes\n'
                '0x100\tguest_register\tEAX:uint32_t\tEAX:uint32_t\t'
                'ebx,ecx,edx,esi,edi,ebp\t0\n')
            with mock.patch.object(ef2000_wcc, 'ROOT', root):
                pragmas = ef2000_wcc.confirmed_abi_pragmas()
            self.assertIn('#pragma aux guest_register parm [eax]', pragmas)
            self.assertNotIn('#pragma aux guest_stack ', pragmas)
            self.assertNotIn('#pragma aux guest_stack_literal ', pragmas)

    def test_compiles_fixture_with_pinned_flags(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'smoke.obj'
            def fake_compiler(argv, check):
                self.assertTrue(check)
                self.assertEqual(argv[0], '/example/wcc386')
                for flag in ef2000_wcc.FLAGS:
                    self.assertIn(flag, argv)
                self.assertIn('-s', argv)  # No unresolved __CHK dependency.
                self.assertIn('-3r', argv)
                self.assertIn('-fpi87', argv)
                self.assertIn('-fp3', argv)
                self.assertIn('-zp1', argv)
                self.assertNotIn('-on', argv)  # No numerically unstable math.
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
