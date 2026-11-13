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

    def test_control_and_flag_operations_with_seeded_x87_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for index, source in enumerate((
                    'ret', 'fninit\nret', 'fnclex\nret', 'fnstsw ax\nret',
                    'fnstcw word ptr [0x300200]\nfnstsw word ptr [0x300202]\nret',
                    'mov word ptr [0x300100],0x27f\nfldcw word ptr [0x300100]\nret',
                    'mov word ptr [0x300100],0xffff\nfldcw word ptr [0x300100]\nret',
                    'sahf\nret', 'lahf\nret')):
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
                                      initial_x87=dict(seeded_x87(), x87_opcode=0,
                                          x87_status=0xedff if source.startswith('fnclex') else 0x6d21))
                if source.startswith('fnstsw ax'):
                    self.assertEqual(result, dict(state='mismatch', differences=[
                        'x87_ip: C=0x123456 x86=0x100']))
                elif '0xffff' in source:
                    self.assertEqual(result, dict(state='mismatch', differences=[
                        'x87_control: C=0x1f7f x86=0xffff']))
                else:
                    self.assertEqual(result, dict(state='pass', differences=[]), source)
                if source.startswith('fninit'):
                    result = diff.compare(function, image.read_bytes(), 0x100,
                                          [0, 0, 0, 0, 0x3f0000, 0, 0, 0], 0x202,
                                          initial_x87=seeded_x87())
                    self.assertEqual(result, dict(state='mismatch', differences=[
                        'x87_opcode: C=0x0 x86=0x321']))

    @unittest.skipUnless(platform.machine() == 'x86_64', 'requires x86-64 hardware oracle')
    def test_hardware_control_words_and_status_store_environment(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            code = root / 'hardware.c'
            code.write_text(r'''#include <stdint.h>
void control(uint16_t cw, uint16_t sw, uint16_t *out) {
    unsigned char saved[108];
    uint16_t env[14] = {0x37f,0,sw,0,0xffff,0,0,0,0,0,0,0,0,0};
    __asm__ volatile("fnsave %[save]\n\tfldenv %[env]\n\tfldcw %[cw]\n\t"
                     "fnstcw (%[out])\n\tfnstsw 2(%[out])\n\tfnclex\n\tfrstor %[save]"
                     : [save] "+m"(saved)
                     : [env] "m"(env), [cw] "m"(cw), [out] "r"(out) : "memory");
}
void status_env(const void *initial, void *before, void *after) {
    unsigned char saved[108];
    __asm__ volatile("fnsave %[save]\n\tfldenv (%[initial])\n\tfnstenv (%[before])\n\t"
                     "fnstsw %%ax\n\tfnstenv (%[after])\n\tfnclex\n\tfrstor %[save]"
                     : [save] "+m"(saved)
                     : [initial] "r"(initial), [before] "r"(before), [after] "r"(after)
                     : "rax", "memory");
}
''')
            library = root / 'hardware.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(code), '-o', str(library)], check=True)
            hardware = ctypes.CDLL(str(library))
            control = hardware.control
            control.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16)]
            control.restype = None
            image, record = assemble(root, 'fldcw word ptr [0x500]\nret')
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            lifted = root / 'lift.c'
            lifted.write_text(asm_to_c.translate(record))
            shared = root / 'lift.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
                            '-Werror', '-Wno-unused-function', str(lifted), '-o', str(shared)], check=True)
            function = ctypes.CDLL(str(shared)).ef_lift_00000100
            function.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint32]
            function.restype = ctypes.c_uint
            memory = bytearray(4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            native = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            state = diff.State((ctypes.c_uint32 * 8)(), 0xad7, 0x100, 0, native, len(memory))
            result = (ctypes.c_uint16 * 2)()
            for status in range(64):
                words = range(65536) if status in (0, 0x21) else (0x340 | mask for mask in range(64))
                for word in words:
                    control(word, status, result)
                    memory[0x500:0x502] = word.to_bytes(2, 'little')
                    state.pc, state.r[4] = 0x100, 0x800
                    state.x87_control, state.x87_status = 0x37f, status
                    self.assertEqual(function(ctypes.byref(state), 10), 0)
                    self.assertEqual((state.x87_control, state.x87_status), tuple(result), (word, status))
            self.assertEqual(state.flags, 0xad7)
            # Pending #MF delivery is explicit and must not change state.
            state.pc, state.r[4], state.x87_status = 0x100, 0x800, 0x80a1
            previous = bytes(state)
            self.assertEqual(function(ctypes.byref(state), 10), 8)
            self.assertEqual(bytes(state), previous)

            status_env = hardware.status_env
            status_env.argtypes = [ctypes.c_void_p] * 3
            status_env.restype = None
            initial = bytearray(28)
            initial[0:2], initial[8:10] = b'\x7f\x03', b'\xff\xff'
            initial[12:16] = (0x123456).to_bytes(4, 'little')
            source = (ctypes.c_uint8 * 28).from_buffer(initial)
            before, after = (ctypes.c_uint8 * 28)(), (ctypes.c_uint8 * 28)()
            status_env(source, before, after)
            self.assertEqual(int.from_bytes(bytes(before)[12:16], 'little'), 0x123456)
            self.assertEqual(bytes(before), bytes(after))

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
