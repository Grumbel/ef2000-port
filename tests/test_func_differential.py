# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free ABI/test-spec checks; execute synthetic code if Unicorn exists."""

import importlib.util
import json
from pathlib import Path
import struct
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
    def test_hiscore_candidate_spec_covers_signed_boundaries(self):
        path = ROOT / 'decomp/tests/0x000432c.json'
        signature = differential.read_signature(ROOT / 'decomp/symbols/signatures.tsv',
                                                 0x432c)
        spec = json.loads(path.read_text())
        _, _, _, _, _, size, cases = differential.validate_spec(spec, signature, 1662896)
        self.assertEqual(size, 15)
        self.assertEqual(len(cases), 10)
        outcomes = set()
        for _, _, _, memory in cases:
            self.assertEqual([address for address, _ in memory], [0x1970a0, 0x19719a])
            current, tenth = [struct.unpack('<i', data)[0] for _, data in memory]
            outcomes.add(current > tenth)
        self.assertEqual(outcomes, {True, False})

    def test_spatial_and_indexed_candidates_have_reviewed_guest_specs(self):
        signatures = ROOT / 'decomp/symbols/signatures.tsv'
        candidates = {
            0x263e0: (12, 5),
            0x263ec: (55, 5),
            0x6a1b4: (26, 10),
            0x6a354: (34, 4),
            0x6a378: (27, 4),
            0x6a394: (57, 5),
            0x6a3d0: (18, 4),
            0x7489c: (93, 9),
            0x238a0: (31, 1),
            0x2541c: (77, 3),
            0x2b884: (58, 3),
            0x2b910: (75, 2),
            0x2b95c: (100, 3),
            0x2ba14: (86, 3),
            0x2b558: (101, 3),
            0x2beb0: (85, 3),
            0x2bf08: (95, 3),
            0x2dba4: (82, 4),
            0x2e030: (90, 2),
            0x2e910: (57, 8),
            0x2afcc: (171, 5),
            0x2b5c0: (116, 3),
            0x2b634: (91, 2),
            0x2b690: (118, 2),
            0x2e08c: (201, 3),
        }
        for address, (expected_size, expected_cases) in candidates.items():
            with self.subTest(address=f'0x{address:x}'):
                signature = differential.read_signature(signatures, address)
                spec = json.loads((ROOT / f'decomp/tests/0x{address:08x}.json').read_text())
                validated = differential.validate_spec(spec, signature, 1662896)
                self.assertEqual(validated[5], expected_size)
                self.assertEqual(len(validated[6]), expected_cases)
                self.assertEqual(len({case[0] for case in validated[6]}), expected_cases)

        self.assertIn('edx', differential.read_signature(signatures, 0x263e0).preserved)
        self.assertNotIn('edx', differential.read_signature(signatures, 0x263ec).preserved)
        self.assertEqual(differential.read_signature(signatures, 0x6a1b4).outputs,
                         ('EAX',))
        self.assertEqual(differential.read_signature(signatures, 0x7489c).outputs,
                         ('EAX',))

    def test_reviewed_signatures_and_subregisters(self):
        path = ROOT / 'decomp/symbols/signatures.tsv'
        signature = differential.read_signature(path, 0x432c)
        self.assertEqual(signature.name, 'hiscore_qualifies')
        self.assertEqual(signature.outputs, ('AL',))
        self.assertEqual(differential.get_part({'eax': 0xabcd2345}, 'al'), 0x45)
        self.assertEqual(differential.get_part({'eax': 0xabcd2345}, 'ax'), 0x2345)
        with self.assertRaisesRegex(ValueError, 'x87 not yet supported'):
            differential.read_signature(path, 0xf8250)
        # The high-score leaf has since advanced from a named candidate to
        # a confirmed, compiled draft; this check must follow the tracker.
        self.assertEqual(differential.read_function_extent(
            ROOT / 'decomp/symbols/functions.tsv', 0x432c), 15)
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
