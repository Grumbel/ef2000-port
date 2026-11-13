# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Hardware-backed exact widening and exception tests for FLD m32/m64."""

import ctypes
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_x87, asm_c_differential as diff
from tests.test_asm_to_c import assemble


@unittest.skipUnless(platform.machine() == 'x86_64' and
                     all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires x86-64, C compiler and binutils')
class RealLoadTests(unittest.TestCase):
    def test_saved_opcode_for_sib_and_absolute_address_forms(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            code = root / 'opcodes.c'
            code.write_text(r'''#include <stdint.h>
unsigned sib(void) {
 unsigned char saved[108], out[108]; uint64_t data = 0;
 __asm__ volatile("fnsave %[save]\n\tfldl (%%rax,%%rdx,1)\n\tfnsave %[out]\n\tfrstor %[save]"
 : [save] "+m"(saved),[out] "=m"(out) : "a"(&data),"d"((uintptr_t)0) : "memory");
 return (out[18] | ((unsigned)out[19] << 8)) & 0x7ff;
}
unsigned displacement(void) {
 unsigned char saved[108], out[108];
 __asm__ volatile("fnsave %[save]\n\tfldl 1f(%%rip)\n\tjmp 2f\n\t1: .quad 0\n\t"
                 "2: fnsave %[out]\n\tfrstor %[save]"
 : [save] "+m"(saved),[out] "=m"(out) : : "memory");
 return (out[18] | ((unsigned)out[19] << 8)) & 0x7ff;
}
''')
            library = root / 'opcodes.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(code), '-o', str(library)], check=True)
            hardware = ctypes.CDLL(str(library))
            for name, address in [('sib', '[esi+edi*2]'), ('displacement', '[0x500]')]:
                _, record = assemble(root, 'fld qword ptr ' + address + '\nret')
                function = getattr(hardware, name)
                function.argtypes, function.restype = [], ctypes.c_uint
                self.assertEqual(asm_x87.memory_encoding(record['instructions'][0], 0xdd, 0)[1], function())

    def test_widening_modes_masks_stack_faults_and_all_exponents(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            native_sources, lifted = [], []
            for width, qualifier in [(32, 'dword'), (64, 'qword')]:
                _, record = assemble(root, 'fld %s ptr [esi]\nret' % qualifier)
                opcode = ','.join(hex(v) for v in bytes.fromhex(record['instructions'][0]['bytes']))
                lifted.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'load%d' % width))
                native_sources.append('''#include <stdint.h>
uintptr_t native%d(const void *initial, void *out, void *data) {
 unsigned char saved[108]; uintptr_t pc;
 __asm__ volatile("fnsave %%[save]\\n\\tfrstor (%%[initial])\\n\\t1: .byte %s\\n\\t"
 "fnsave (%%[out])\\n\\tfrstor %%[save]\\n\\tleaq 1b(%%%%rip),%%[pc]"
 : [save] "+m"(saved),[pc] "=r"(pc) : [initial] "r"(initial),[out] "r"(out),"S"(data) : "memory");
 return pc;
}''' % (width, opcode))
            for name, source in [('native', native_sources), ('lifted', lifted)]:
                path = root / (name + '.c')
                path.write_text('\n'.join(source))
                command = ['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                           '-Wno-unused-function', str(path), '-o', str(root / (name + '.so'))]
                if name == 'lifted':
                    command += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
                subprocess.run(command, check=True)
            hardware, generated = ctypes.CDLL(str(root / 'native.so')), ctypes.CDLL(str(root / 'lifted.so'))
            memory = bytearray(4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            template = diff.State((ctypes.c_uint32 * 8)(), 0xad7, 0x100, 0, ram, len(memory))
            template.r[4], template.r[6] = 0x800, 0x500
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_ip, template.x87_cs, template.x87_opcode = 0x123456, 16, 0x321
            template.x87_dp, template.x87_ds = 0x234567, 16
            values = [(0x8000000000000000 + i, 0x3fff) for i in range(8)]
            for index, (significand, exponent) in enumerate(values):
                template.x87[index][:] = [significand & 0xffffffff, significand >> 32, exponent]
            saved_template = bytes(template)
            output = (ctypes.c_uint8 * 108)()
            data = (ctypes.c_uint8 * 8)()
            cases = 0
            for width, fraction_bits, exponent_bits in [(32, 23, 8), (64, 52, 11)]:
                native, function = getattr(hardware, 'native%d' % width), getattr(generated, 'load%d' % width)
                native.argtypes, native.restype = [ctypes.c_void_p] * 3, ctypes.c_uint64
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                def check(bits, control, top, occupied, full=True):
                    nonlocal cases
                    initial = bytearray(108)
                    status = 0x4700 | (top << 11)
                    tags = sum((0 if occupied & (1 << i) else 3) << (i*2) for i in range(8))
                    struct.pack_into('<H', initial, 0, control)
                    struct.pack_into('<H', initial, 4, status)
                    struct.pack_into('<H', initial, 8, tags)
                    struct.pack_into('<IHHIH', initial, 12, 0x123456, 16, 0x321, 0x234567, 16)
                    for logical in range(8):
                        struct.pack_into('<QH', initial, 28+logical*10, *values[(top+logical)&7])
                    encoded = bits.to_bytes(width//8, 'little')
                    data[:width//8] = encoded
                    memory[0x500:0x500+width//8] = encoded
                    pc = native((ctypes.c_uint8 * 108).from_buffer(initial), output, data)
                    state = diff.State.from_buffer_copy(saved_template)
                    state.x87_control, state.x87_status, state.x87_tag = control, status, tags
                    context = (width, hex(bits), hex(control), top, hex(tags))
                    self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                    observed = bytes(output)
                    expected_status = struct.unpack_from('<H', observed, 4)[0]
                    self.assertEqual((state.x87_status, state.x87_tag),
                                     (expected_status, struct.unpack_from('<H', observed, 8)[0]), context)
                    expected_top = (expected_status >> 11) & 7
                    indices = range(8) if full else [expected_top]
                    for physical in indices:
                        row = state.x87[physical]
                        self.assertEqual((row[0] | (row[1] << 32), row[2]),
                            struct.unpack_from('<QH', observed, 28+((physical-expected_top)&7)*10), context)
                    if full:
                        self.assertEqual(struct.unpack_from('<I', observed, 12)[0], pc & 0xffffffff)
                        self.assertEqual(struct.unpack_from('<I', observed, 20)[0], ctypes.addressof(data) & 0xffffffff)
                        self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                         (0x100, 8, 0x500, 16), context)
                        self.assertEqual(state.x87_opcode, struct.unpack_from('<H', observed, 18)[0] & 0x7ff)
                        self.assertEqual((state.pc, state.r[4], state.flags, state.x87_control),
                                         (0xf00, 0x804, 0xad7, control), context)
                        self.assertEqual(bytes(memory[0x500:0x500+width//8]), encoded)
                    cases += 1
                maximum = (1 << exponent_bits) - 1
                mantissa_mask, quiet = (1 << fraction_bits) - 1, 1 << (fraction_bits-1)
                sign = 1 << (width-1)
                special = [0, sign, 1, mantissa_mask, 1 << fraction_bits,
                           ((maximum-1) << fraction_bits) | mantissa_mask,
                           maximum << fraction_bits, sign | (maximum << fraction_bits),
                           (maximum << fraction_bits) | 1, sign | (maximum << fraction_bits) | 1,
                           (maximum << fraction_bits) | quiet, sign | (maximum << fraction_bits) | quiet]
                for control in (0x37c, 0x37d, 0x37e, 0x37f):
                    for top in range(8):
                        for occupied in range(256):
                            for bits in special:
                                check(bits, control, top, occupied)
                # Loads widen exactly in all precision/rounding modes; each
                # exponent, sign and key mantissa boundary is exercised.
                for precision in (0, 2, 3):
                    for rounding in range(4):
                        control = 0x7f | (precision << 8) | (rounding << 10)
                        for exponent in range(maximum+1):
                            for fraction in (0, 1, quiet-1, quiet, mantissa_mask):
                                for sign_bit in (0, sign):
                                    check(sign_bit | (exponent << fraction_bits) | fraction,
                                          control, 0, 0, full=False)
                # Bounds precede stack/data exceptions and do not mutate FP state.
                for address, size in [(4096-width//8+1,4096),(0,width//8-1),(0xffffffff,4096)]:
                    state = diff.State.from_buffer_copy(saved_template)
                    state.x87_control, state.x87_tag = 0x37e, 0
                    state.r[6], state.memory_size = address, size
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                    state.x87_status = 0x8081
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 8)
                    self.assertEqual(bytes(state), before)
                state = diff.State.from_buffer_copy(saved_template)
                state.r[6], state.x87_control, state.x87_tag = 4096-width//8, 0x37f, 0xffff
                one = (127 if width == 32 else 1023) << fraction_bits
                memory[4096-width//8:] = one.to_bytes(width//8, 'little')
                self.assertEqual(function(ctypes.byref(state), 10), 0)
                self.assertEqual(list(state.x87[7]), [0, 0x80000000, 0x3fff])
                self.assertEqual(state.x87_dp, 4096-width//8)
            self.assertEqual(cases, 473088)
