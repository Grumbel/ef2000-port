# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free OMF -> reviewed relocation -> EFPATCH1 -> guest execution."""

import importlib.util
import unittest

from tools import efpatch, func_differential, omf_inplace
from tools.flirt_match import OMFError


def record(kind, payload):
    length = len(payload) + 1
    header = bytes([kind]) + length.to_bytes(2, 'little')
    return header + payload + bytes([(-sum(header + payload)) & 0xff])


def symbol(value):
    return bytes([len(value)]) + value.encode('ascii')


def omf_object(code=b'\xa1\x00\x00\x00\x00\x40\xc3', public_name='ef_foo_'):
    # A single USE32 CODE segment, one public and one external absolute
    # 32-bit relocation: mov eax, [g_score]; inc eax; ret.
    return b''.join((
        record(0x80, symbol('synthetic')),
        record(0x96, symbol('_TEXT') + symbol('CODE')),
        record(0x99, b'\x61' + len(code).to_bytes(4, 'little') + b'\x01\x02\x00'),
        record(0x8c, symbol('g_score_') + b'\x00'),
        record(0x91, b'\x00\x01' + symbol(public_name) + b'\x00\x00\x00\x00\x00'),
        record(0xa1, b'\x01\x00\x00\x00\x00' + code),
        record(0x9d, b'\xe4\x01\x56\x01'),  # absolute 32-bit external fixup
        record(0x8a, b'\x00'),
    ))


def inputs():
    original = bytearray(0x200)
    original[0x20:0x29] = bytes.fromhex('a1 00010000 83c001 c3')
    original[0x100:0x104] = (41).to_bytes(4, 'little')
    functions = [{'addr': '0x00000020', 'size': '16', 'name': 'ef_foo',
                  'state': 'compiles', 'sig_status': 'confirmed'}]
    globals_ = [{'addr': '0x00000100', 'name': 'g_score'}]
    return bytes(original), functions, globals_


class InPlacePatchTests(unittest.TestCase):
    def test_cdecl_leading_underscore_public_matches_canonical_function(self):
        original, functions, globals_ = inputs()
        blob, entry = omf_inplace.build(omf_object(public_name='_ef_foo'),
                                        original, functions, globals_, {0x20})
        self.assertEqual(entry['offset'], 0x20)
        self.assertEqual(efpatch.decode(blob, original)[0][1][:7],
                         bytes.fromhex('a1 00010000 40 c3'))

    def test_full_conversion_resolves_absolute_guest_global(self):
        original, functions, globals_ = inputs()
        blob, entry = omf_inplace.build(omf_object(), original, functions,
                                        globals_, {0x20})
        self.assertEqual(entry['kind'], 'inline')
        self.assertEqual(entry['offset'], 0x20)
        writes = efpatch.decode(blob, original)
        self.assertEqual(len(writes), 1)
        self.assertEqual(writes[0][0], 0x20)
        self.assertEqual(writes[0][1], bytes.fromhex('a1 00010000 40 c3') + b'\xcc' * 9)
        with self.assertRaisesRegex(efpatch.PatchError, 'different initialized payload'):
            efpatch.decode(blob, bytes(512))

    def test_fails_on_unreviewed_drafted_and_oversized_code(self):
        original, functions, globals_ = inputs()
        with self.assertRaisesRegex(OMFError, 'reviewed control-flow'):
            omf_inplace.build(omf_object(), original, functions, globals_, set())
        draft = [dict(row, state='drafted') for row in functions]
        with self.assertRaisesRegex(OMFError, 'confirmed ABI and compiles'):
            omf_inplace.build(omf_object(), original, draft, globals_, {0x20})
        with self.assertRaisesRegex(OMFError, 'canonical addresses'):
            omf_inplace.build(omf_object(), original, functions, [], {0x20})
        long_code = b'\xa1\x00\x00\x00\x00\x40\xc3' + b'\x90' * 15
        with self.assertRaisesRegex(efpatch.PatchError, 'arena/redirected'):
            omf_inplace.build(omf_object(long_code), original, functions, globals_, {0x20})

    def test_noncontiguous_ghidra_body_is_not_an_inplace_capacity(self):
        original, functions, globals_ = inputs()
        unsafe = [dict(row, notes='Noncontiguous 7+9 bytes; shared epilogue')
                  for row in functions]
        with self.assertRaisesRegex(OMFError, 'noncontiguous function body'):
            omf_inplace.build(omf_object(), original, unsafe, globals_, {0x20})

    @unittest.skipUnless(importlib.util.find_spec('unicorn'), 'requires Python Unicorn')
    def test_isolated_original_vs_relocated_patch_execution(self):
        original, functions, globals_ = inputs()
        blob, _ = omf_inplace.build(omf_object(), original, functions, globals_, {0x20})
        patched = bytearray(original)
        for address, payload in efpatch.decode(blob, original):
            patched[address:address + len(payload)] = payload
        sig = func_differential.Signature(0x20, 'ef_foo', ('eax',),
                                          ('ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'), 0)
        case = ('score_41', {}, [], [])
        diffs, blocks, _ = func_differential.compare_case(
            original, bytes(patched), sig, case, 0x5000, 0x3000, 0x4000,
            0x1000, 128, 16)
        self.assertEqual(diffs, [])
        self.assertTrue(blocks)


if __name__ == '__main__':
    unittest.main()
