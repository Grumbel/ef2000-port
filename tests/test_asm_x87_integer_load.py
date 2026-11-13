# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Hardware comparisons for exact signed integer x87 loads."""

import ctypes
import multiprocessing
import resource
import signal
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff
from tests.test_asm_to_c import assemble


@unittest.skipUnless(platform.machine() == 'x86_64' and
                     all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires x86-64, C compiler and binutils')
class IntegerLoadTests(unittest.TestCase):
    def test_signed_values_modes_and_stack_state_against_hardware(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            native_sources, lifted = [], []
            for width, qualifier in [(16, 'word'), (32, 'dword'), (64, 'qword')]:
                _, record = assemble(root, 'fild %s ptr [esi]\nret' % qualifier)
                opcode = ','.join(hex(v) for v in bytes.fromhex(record['instructions'][0]['bytes']))
                lifted.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'load%d' % width))
                native_sources.append('''#include <stdint.h>
uintptr_t native%d(const void *initial,void *out,void *data) {
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
            values = [(0x8000000000000000+i, 0x3fff) for i in range(8)]
            for physical, (significand, exponent) in enumerate(values):
                template.x87[physical][:] = [significand & 0xffffffff, significand >> 32, exponent]
            saved_template = bytes(template)
            output, data = (ctypes.c_uint8 * 108)(), (ctypes.c_uint8 * 8)()
            cases = 0
            for width in (16, 32, 64):
                native, function = getattr(hardware, 'native%d' % width), getattr(generated, 'load%d' % width)
                native.argtypes, native.restype = [ctypes.c_void_p] * 3, ctypes.c_uint64
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                mask = (1 << width)-1
                def check(number, control, top=0, occupied=0, full=False):
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
                    encoded = (number & mask).to_bytes(width//8, 'little')
                    data[:width//8] = encoded
                    memory[0x500:0x500+width//8] = encoded
                    pc = native((ctypes.c_uint8 * 108).from_buffer(initial), output, data)
                    state = diff.State.from_buffer_copy(saved_template)
                    state.x87_control, state.x87_status, state.x87_tag = control, status, tags
                    context = (width, number, hex(control), top, hex(tags))
                    self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                    observed = bytes(output)
                    expected_status = struct.unpack_from('<H', observed, 4)[0]
                    expected_top = (expected_status >> 11) & 7
                    self.assertEqual((state.x87_status, state.x87_tag),
                                     (expected_status, struct.unpack_from('<H', observed, 8)[0]), context)
                    for physical in (range(8) if full else [expected_top]):
                        row = state.x87[physical]
                        self.assertEqual((row[0] | (row[1] << 32), row[2]),
                            struct.unpack_from('<QH', observed, 28+((physical-expected_top)&7)*10), context)
                    if full:
                        self.assertEqual(struct.unpack_from('<I', observed, 12)[0], pc & 0xffffffff)
                        self.assertEqual(struct.unpack_from('<I', observed, 20)[0], ctypes.addressof(data) & 0xffffffff)
                        self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                         (0x100, 8, 0x500, 16), context)
                        self.assertEqual(state.x87_opcode, struct.unpack_from('<H', observed, 18)[0] & 0x7ff)
                        self.assertEqual((state.flags, state.pc, state.r[4], state.x87_control),
                                         (0xad7, 0xf00, 0x804, control), context)
                        self.assertEqual(bytes(memory[0x500:0x500+width//8]), encoded)
                    cases += 1
                minimum, maximum = -(1 << (width-1)), (1 << (width-1))-1
                if width == 16:
                    numbers = range(minimum, maximum+1)
                else:
                    numbers = {minimum, maximum, 0, 1, -1}
                    for bit in range(width-1):
                        base = 1 << bit
                        numbers.update(v for v in (base, -base, base-1, base+1, -base-1, -base+1)
                                       if minimum <= v <= maximum)
                for precision in (0, 2, 3):
                    for rounding in range(4):
                        control = 0x7f | (precision << 8) | (rounding << 10)
                        for number in numbers:
                            check(number, control)
                for control in (0x37e, 0x37f):
                    for top in range(8):
                        for occupied in range(256):
                            for number in (0, 1, -1, minimum, maximum, minimum+1, maximum-1):
                                check(number, control, top, occupied, full=True)
                for address, size in [(4096-width//8+1,4096),(0,width//8-1),(0xffffffff,4096)]:
                    state = diff.State.from_buffer_copy(saved_template)
                    state.r[6], state.memory_size = address, size
                    state.x87_control, state.x87_tag = 0x37e, 0
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                    state.x87_status = 0x80c1
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 8)
                    self.assertEqual(bytes(state), before)
                state = diff.State.from_buffer_copy(saved_template)
                state.r[6], state.x87_control, state.x87_tag = 4096-width//8, 0x37f, 0xffff
                memory[4096-width//8:] = (minimum & mask).to_bytes(width//8, 'little')
                self.assertEqual(function(ctypes.byref(state), 10), 0)
                self.assertEqual(list(state.x87[7]), [0, 0x80000000, 0x8000 | (16383+width-1)])
                for pending in (False, True):
                    for masked in (False, True):
                        for tags in (0, 0xffff):
                            def fault():
                                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                                initial = bytearray(108)
                                struct.pack_into('<H', initial, 0, 0x37e | (masked and not pending))
                                struct.pack_into('<H', initial, 4, 0x80c1 if pending else 0)
                                struct.pack_into('<H', initial, 8, tags)
                                native((ctypes.c_uint8 * 108).from_buffer(initial), output, None)
                            process = multiprocessing.get_context('fork').Process(target=fault)
                            process.start()
                            process.join()
                            self.assertEqual(process.exitcode, -signal.SIGFPE if pending else -signal.SIGSEGV)
            self.assertEqual(cases, 879096)
