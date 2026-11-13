# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Explicit raw x87 state checks; synthetic programs, no game data."""

import ctypes
import json
import platform
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff
from tests.test_asm_to_c import assemble, unicorn


def seeded_x87():
    return dict(x87=[[0x89abc000 + i, 0x81234567, 0x3fff | ((i & 1) << 15)] for i in range(8)],
                x87_control=0xf7f, x87_status=0x6d21, x87_tag=0xcccc,
                x87_ip=0x123456, x87_cs=8, x87_dp=0x234567, x87_ds=16, x87_opcode=0x321)


@unittest.skipUnless(unicorn and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Unicorn, binutils and C compiler')
class X87StateTests(unittest.TestCase):
    def test_old_state_abi_is_rejected_before_compilation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / 'image.bin'
            image.write_bytes(b'')
            (root / 'report.json').write_text(json.dumps(dict(state_abi=1)))
            with self.assertRaisesRegex(ValueError, 'incompatible translation state ABI'):
                diff.verify(root, root / 'evidence', image, compiler='must-not-be-invoked')
            self.assertFalse((root / 'evidence').exists())

    def test_snapshot_preserves_physical_registers_and_environment(self):
        x = unicorn.x86_const
        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        original = diff.bind_x87(cpu, seeded_x87())
        self.assertEqual(original, seeded_x87())
        state = diff.State()
        diff.set_x87(state, original)
        self.assertEqual([list(words) for words in state.x87], original['x87'])
        for field in diff.X87_FIELDS:
            self.assertEqual(getattr(state, field), original[field])
        self.assertNotEqual(cpu.reg_read(x.UC_X86_REG_FP0), cpu.reg_read(x.UC_X86_REG_FP0 + 5))

    def test_ret_preserves_and_fninit_resets_environment_without_erasing_registers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for index, source in enumerate(('ret', 'fninit\nret')):
                image, record = assemble(root, source)
                code = root / ('lift%d.c' % index)
                code.write_text(asm_to_c.translate(record))
                library = code.with_suffix('.so')
                subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
                                '-Werror', '-Wno-unused-function', str(code), '-o', str(library)], check=True)
                function = ctypes.CDLL(str(library)).ef_lift_00000100
                function.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint32]
                function.restype = ctypes.c_uint
                result = diff.compare(function, image.read_bytes(), 0x100,
                                      [17, 19, 23, 29, 0x3f0000, 31, 37, 41], 0xad7,
                                      initial_x87=dict(seeded_x87(), x87_opcode=0))
                self.assertEqual(result, dict(state='pass', differences=[]))
                if source.startswith('fninit'):
                    result = diff.compare(function, image.read_bytes(), 0x100,
                                          [0, 0, 0, 0, 0x3f0000, 0, 0, 0], 0x202,
                                          initial_x87=seeded_x87())
                    self.assertEqual(result, dict(state='mismatch', differences=[
                        'x87_opcode: C=0x0 x86=0x321']))

    @unittest.skipUnless(platform.machine() == 'x86_64', 'requires x86-64 hardware oracle')
    def test_hardware_fninit_clears_seeded_opcode(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            code = root / 'reset.c'
            code.write_text(r'''void probe(const void *initial, void *before, void *after) {
    unsigned char saved[108];
    __asm__ volatile("fnsave %[save]\n\tfldenv (%[initial])\n\tfnstenv (%[before])\n\t"
                     "fninit\n\tfnstenv (%[after])\n\tfrstor %[save]"
                     : [save] "+m"(saved)
                     : [initial] "r"(initial), [before] "r"(before), [after] "r"(after)
                     : "memory");
}
''')
            library = root / 'reset.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(code), '-o', str(library)], check=True)
            function = ctypes.CDLL(str(library)).probe
            function.argtypes = [ctypes.c_void_p] * 3
            function.restype = None
            initial = bytearray(28)
            initial[0:2], initial[8:10], initial[18:20] = b'\x7f\x03', b'\xff\xff', b'\x21\x03'
            source = (ctypes.c_uint8 * 28).from_buffer(initial)
            before, after = (ctypes.c_uint8 * 28)(), (ctypes.c_uint8 * 28)()
            function(source, before, after)
            self.assertEqual(int.from_bytes(bytes(before)[18:20], 'little') & 0x7ff, 0x321)
            self.assertEqual(int.from_bytes(bytes(after)[18:20], 'little') & 0x7ff, 0)

    def test_differential_rejects_every_x87_field_and_empty_register_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            image, _ = assemble(Path(temporary), 'ret')
            for field in list(diff.X87_FIELDS) + ['x87']:
                def corrupt(pointer, budget):
                    state = ctypes.cast(pointer, ctypes.POINTER(diff.State)).contents
                    state.pc, state.r[4] = 0x3ff000, state.r[4] + 4
                    if field == 'x87':
                        # Physical FP0 is empty in this seeded tag word. Its
                        # retained payload still belongs to compared state.
                        state.x87[0][0] ^= 1
                    else:
                        setattr(state, field, getattr(state, field) ^ 1)
                    return 0
                with self.subTest(field=field):
                    result = diff.compare(corrupt, image.read_bytes(), 0x100,
                                          [0, 0, 0, 0, 0x3f0000, 0, 0, 0], 0x202,
                                          initial_x87=seeded_x87())
                    self.assertEqual(result['state'], 'mismatch')
                    self.assertTrue(any(field in message for message in result['differences']), result)
