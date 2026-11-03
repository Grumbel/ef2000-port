# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free ABI/test-spec checks; execute synthetic code if Unicorn exists."""

import importlib.util
from pathlib import Path
import tempfile
import unittest

from tools import func_differential as differential
from tools.run_startup import analyze_calls


ROOT = Path(__file__).resolve().parents[1]


def fixture():
    signature = differential.Signature(0x100, 'synthetic_increment',
                                       ('EAX',), ('ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'), 0)
    spec = {
        'address': '0x100', 'function_size': 15,
        'guest_size': '0x5000', 'stack_top': '0x3000',
        'return_sentinel': '0x2000', 'stack_scratch_bytes': '0x1000',
        'max_instructions': 500,
        'cases': [{'name': 'increment_41', 'registers': {'edx': '0x87654321'},
                   'memory': [{'address': '0x1800', 'hex': '29000000'}]}],
    }
    original = bytearray(0x2000)
    original[0x100:0x10c] = bytes.fromhex('a1 00 18 00 00 40 a3 00 18 00 00 c3')
    patched = bytearray(original)
    patched[0x100:0x10e] = bytes.fromhex('a1 00 18 00 00 83 c0 01 a3 00 18 00 00 c3')
    return signature, spec, bytes(original), bytes(patched)


class FunctionDifferentialTests(unittest.TestCase):
    def test_reviewed_signatures_and_subregisters(self):
        path = ROOT / 'decomp/symbols/signatures.tsv'
        signature = differential.read_signature(path, 0x432c)
        self.assertEqual(signature.name, 'hiscore_qualifies')
        self.assertEqual(signature.outputs, ('AL',))
        self.assertEqual(differential.get_part({'eax': 0xabcd2345}, 'al'), 0x45)
        self.assertEqual(differential.get_part({'eax': 0xabcd2345}, 'ax'), 0x2345)
        with self.assertRaisesRegex(ValueError, 'x87 not yet supported'):
            differential.read_signature(path, 0xf8250)
        with self.assertRaisesRegex(ValueError, 'compiled function state'):
            differential.read_function_extent(ROOT / 'decomp/symbols/functions.tsv', 0x432c)
        with tempfile.TemporaryDirectory() as directory:
            functions = Path(directory) / 'functions.tsv'
            functions.write_text('addr\tsize\tsig_status\tstate\n'
                                 '0x100\t15\tconfirmed\tcompiles\n')
            self.assertEqual(differential.read_function_extent(functions, 0x100), 15)

    def test_spec_validation_rejects_unreviewed_ranges_and_abi(self):
        signature, spec, original, _ = fixture()
        validated = differential.validate_spec(spec, signature, len(original))
        self.assertEqual(validated[5], 15)
        self.assertEqual(validated[6][0][1]['edx'], 0x87654321)
        bad = dict(spec, address='0x101')
        with self.assertRaisesRegex(ValueError, 'addresses differ'):
            differential.validate_spec(bad, signature, len(original))
        bad = dict(spec, function_size=0)
        with self.assertRaisesRegex(ValueError, 'invalid guest'):
            differential.validate_spec(bad, signature, len(original))
        bad = dict(spec, cases=[{'name': 'bad-stack', 'stack_args': [1]}])
        with self.assertRaisesRegex(ValueError, 'stack arguments'):
            differential.validate_spec(bad, signature, len(original))
        bad = dict(spec, cases=[{'name': 'bad', 'memory': [
            {'address': '0x2fff', 'hex': '1234'}]}])
        with self.assertRaisesRegex(ValueError, 'overlaps scratch'):
            differential.validate_spec(bad, signature, len(original))

    def test_written_ram_excludes_only_callee_scratch(self):
        touched = [(0x100, 0x110), (0x1f80, 0x2080), (0x2ff0, 0x3010)]
        visible = differential.visible_ranges(touched, 0x3000, 0x1000)
        self.assertEqual(visible, [(0x100, 0x110), (0x1f80, 0x2000),
                                   (0x3000, 0x3010)])
        self.assertEqual(differential.merged_ranges([(3, 5), (0, 2), (2, 3)]), [(0, 5)])

    def test_native_memory_watermark_in_run_report(self):
        report = analyze_calls('MEMORY HEAP_PEAK=00500000 STACK_LOW=03ffd000 '
                               'HEAP_END=00500000 STACK_END=03ffd000 '
                               'FREE_BYTES=03a00000 HEAP_CALLS=00000002 STACK_CALLS=00000001\n')
        self.assertEqual(report['memory_watermark']['HEAP_PEAK'], 0x500000)
        self.assertEqual(report['memory_watermark']['STACK_CALLS'], 1)

    @unittest.skipUnless(importlib.util.find_spec('unicorn'), 'requires Python Unicorn')
    def test_synthetic_original_vs_patch_and_divergence(self):
        signature, spec, original, patched = fixture()
        guest, stack, sentinel, scratch, budget, size, cases = differential.validate_spec(
            spec, signature, len(original))
        differences, blocks, written = differential.compare_case(
            original, patched, signature, cases[0], guest, stack, sentinel,
            scratch, budget, size)
        self.assertEqual(differences, [])
        self.assertGreaterEqual(len(blocks), 1)
        self.assertEqual(written, 4)
        broken = bytearray(patched)
        broken[0x107] = 2  # add eax, 2 rather than add eax, 1
        differences, _, _ = differential.compare_case(
            original, bytes(broken), signature, cases[0], guest, stack, sentinel,
            scratch, budget, size)
        self.assertTrue(any('EAX' in issue for issue in differences))
        self.assertTrue(any('guest RAM' in issue for issue in differences))


if __name__ == '__main__':
    unittest.main()
