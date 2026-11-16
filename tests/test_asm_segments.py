# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Descriptor rights, privilege and inclusive segment-boundary cases."""

import ctypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_segments, asm_to_c, asm_x87


class Segment(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in ('selector', 'base', 'limit', 'access', 'flags')]


@unittest.skipUnless(shutil.which('cc'), 'requires C compiler')
class SegmentTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('as') and shutil.which('objcopy') and shutil.which('objdump'),
                         'requires GNU binutils')
    def test_operand_segment_matches_independent_modrm_sib_decode(self):
        from tests.test_asm_to_c import assemble
        addresses = ['[eax]', '[ebp]', '[esp]', '[eax+ebp*1]', '[ebp+eax*1]',
                     '[ebp*2+0x1000]', '[esp+esi*4+8]', '[0x1234]']
        forms = addresses + [segment+':'+address
                             for segment in asm_to_c.SEGMENTS for address in addresses]
        with tempfile.TemporaryDirectory() as temporary:
            _, record = assemble(Path(temporary), '\n'.join('fld dword ptr '+form for form in forms)+'\nret')
            instructions = record['instructions'][:-1]
            self.assertEqual(len(instructions), len(forms))
            for form, instruction in zip(forms, instructions):
                with self.subTest(form=form):
                    parsed = asm_to_c.operand(instruction['operands'])
                    encoded_segment, _ = asm_x87.memory_encoding(instruction, 0xd9, 0)
                    self.assertEqual(parsed[3], encoded_segment)
                    self.assertEqual(parsed[2], 32)
        # Segment metadata never changes the offset expression used by LEA.
        self.assertEqual(asm_to_c.operand('fs:[ebp+4]')[1],
                         asm_to_c.operand('ds:[ebp+4]')[1])

    def test_rights_privilege_and_boundaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root/'segments.c'
            source.write_text('#include <stdint.h>\n' + asm_segments.RUNTIME + '''
unsigned probe(const ef_lift_segment *seg, uint32_t cpl, uint32_t offset,
               uint32_t bytes, unsigned kind, uint32_t *linear) {
    return ef_segment_linear(seg, cpl, offset, bytes, kind, linear);
}
''')
            subprocess.run(['cc', '-shared', '-fPIC', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(source), '-o', str(root/'segments.so')], check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'segments.so'))
            probe = library.probe
            probe.argtypes = [ctypes.POINTER(Segment), ctypes.c_uint32, ctypes.c_uint32,
                              ctypes.c_uint32, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint32)]
            probe.restype = ctypes.c_uint
            cases = [
                # access, flags, limit, offset, bytes, kind, CPL, selector, success
                (0xf3, 4, 0xff, 0, 1, 0, 3, 7, True),
                (0xf3, 4, 0xff, 0xfc, 4, 1, 3, 7, True),
                (0xf3, 4, 0xff, 0xfd, 4, 0, 3, 7, False),
                (0xf7, 4, 0x100, 0x100, 1, 3, 3, 7, False),
                (0xf7, 4, 0x100, 0x101, 1, 4, 3, 7, True),
                (0xf7, 4, 0x100, 0xfffffffc, 4, 3, 3, 7, True),
                (0xf7, 4, 0x100, 0xfffffffd, 4, 4, 3, 7, False),
                (0xf7, 0, 0x100, 0xfffe, 2, 4, 3, 7, True),
                (0xf7, 0, 0x100, 0xffff, 2, 3, 3, 7, False),
                (0xf7, 0, 0x100, 0x10000, 1, 3, 3, 7, False),
                (0xf1, 4, 0xffff, 0x123, 1, 0, 3, 7, True),
                (0xf1, 4, 0xffff, 0x123, 1, 1, 3, 7, False),
                (0xf1, 4, 0xffff, 0x123, 1, 3, 3, 7, False),
                (0xfb, 4, 0xffff, 0x123, 1, 2, 3, 15, True),
                (0xfb, 4, 0xffff, 0x123, 1, 0, 3, 15, True),
                (0xfb, 4, 0xffff, 0x123, 1, 1, 3, 15, False),
                (0xfb, 4, 0xffff, 0x123, 1, 3, 3, 15, False),
                (0xf9, 4, 0xffff, 0x123, 1, 0, 3, 15, False),
                (0xf9, 4, 0xffff, 0x123, 1, 2, 3, 15, True),
                (0x9f, 4, 0xffff, 0x123, 1, 2, 3, 15, True),
                (0x9b, 4, 0xffff, 0x123, 1, 2, 3, 15, False),
                (0xf3, 4, 0xffff, 0x123, 1, 4, 0, 7, False),
                (0x93, 4, 0xffff, 0x123, 1, 0, 3, 7, False),
                (0x93, 4, 0xffff, 0x123, 1, 0, 0, 7, False),
                (0x93, 4, 0xffff, 0x123, 1, 0, 0, 8, True),
                (0x73, 4, 0xffff, 0x123, 1, 0, 3, 7, False),
                (0xe3, 4, 0xffff, 0x123, 1, 0, 3, 7, False),
                (0xf3, 4, 0xffff, 0x123, 1, 0, 3, 0, False),
                (0xf3, 4, 0xffff, 0x123, 0, 0, 3, 7, False),
                (0xf3, 4, 0xffff, 0x123, 1, 5, 3, 7, False),
            ]
            for case in cases:
                access, flags, limit, offset, size, kind, cpl, selector, success = case
                segment = Segment(selector, 0x2000, limit, access, flags)
                output = ctypes.c_uint32(0xa5a5a5a5)
                with self.subTest(case=case):
                    result = probe(ctypes.byref(segment), cpl, offset, size, kind, ctypes.byref(output))
                    self.assertEqual(result, 0 if success else 7)
                    self.assertEqual(output.value, (0x2000 + offset) & 0xffffffff if success else 0xa5a5a5a5)

    def test_descriptor_decode_preserves_expand_down_and_granularity(self):
        self.assertEqual(asm_segments.descriptor(7, 0x0040f70000000260),
                         (7, 0, 0x260, 0xf7, 4))
        self.assertEqual(asm_segments.descriptor(15, 0x00cffb000000ffff),
                         (15, 0, 0xffffffff, 0xfb, 12))
        self.assertEqual(asm_segments.descriptor(7, 0x1240f73456780260)[1], 0x12345678)
        for selector, raw in ((0x10000, 0), (0, -1), (0, 1 << 64)):
            with self.assertRaises(ValueError):
                asm_segments.descriptor(selector, raw)


if __name__ == '__main__':
    unittest.main()
