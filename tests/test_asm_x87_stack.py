# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Hardware comparisons for raw x87 stack effects, not instruction environment."""

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
class X87StackTests(unittest.TestCase):
    def test_raw_stack_effects_against_hardware(self):
        operations = [('fld1', 0, 0), ('fldz', 1, 0), ('fchs', 3, 0), ('fabs', 4, 0),
                      ('fincstp', 9, 0), ('fdecstp', 10, 0)]
        for mnemonic, operation in [('fld', 2), ('fxch', 5), ('fst', 6), ('fstp', 7), ('ffree', 8)]:
            operations += [(mnemonic + ' st(%d)' % index, operation, index) for index in range(8)]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'state.h').write_text(asm_to_c.RUNTIME)
            core = root / 'core.c'
            core.write_text('#include "state.h"\n' +
                            '\nunsigned run(ef_lift_state *s, unsigned op, unsigned arg) '
                            '{ return ef_x87_stack(s, op, arg); }\n')
            shared = root / 'core.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
                            '-Werror', '-Wno-unused-function', '-fsanitize=undefined',
                            '-fno-sanitize-recover=all', str(core), '-o', str(shared)], check=True)
            run = ctypes.CDLL(str(shared)).run
            run.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint, ctypes.c_uint]
            run.restype = ctypes.c_uint
            probes, lifted = [], []
            for number, (source, operation, argument) in enumerate(operations):
                _, record = assemble(root, source)
                self.assertEqual(len(record['instructions']), 1)
                opcode = ','.join(hex(value) for value in bytes.fromhex(record['instructions'][0]['bytes']))
                probes.append('void op%d(const void *in, void *out) { unsigned char saved[108]; '
                    '__asm__ volatile("fnsave %%[save]\\n\\tfrstor (%%[in])\\n\\t.byte %s\\n\\t'
                    'fnsave (%%[out])\\n\\tfrstor %%[save]" : [save] "+m"(saved) : '
                    '[in] "r"(in), [out] "r"(out) : "memory"); }' % (number, opcode))
                _, complete = assemble(root, source + '\nret')
                lifted.append(asm_to_c.translate(complete).replace('ef_lift_00000100', 'lift_%d' % number))
            hardware_source = root / 'hardware.c'
            hardware_source.write_text('\n'.join(probes))
            hardware_library = root / 'hardware.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(hardware_source), '-o', str(hardware_library)], check=True)
            hardware = ctypes.CDLL(str(hardware_library))
            lifted_source = root / 'lifted.c'
            lifted_source.write_text('\n'.join(lifted))
            # Each generated body includes this conventional header name.
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            lifted_library = root / 'lifted.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
                            '-Werror', '-Wno-unused-function', '-fsanitize=undefined',
                            '-fno-sanitize-recover=all', str(lifted_source), '-o', str(lifted_library)], check=True)
            generated = ctypes.CDLL(str(lifted_library))
            lifted_functions = []
            functions = []
            for number in range(len(operations)):
                function = getattr(hardware, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.c_void_p] * 2, None
                functions.append(function)
                function = getattr(generated, 'lift_%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                lifted_functions.append(function)
            # Normal, +0, -0, qNaN, infinity, sNaN, denormal, unnormal.
            values = [(0x8000000000000001, 0x3fff), (0, 0), (0, 0x8000),
                      (0xc000000000000003, 0x7fff), (0x8000000000000000, 0x7fff),
                      (0x8000000000000001, 0x7fff), (0x800, 0), (0x4000000000000000, 0x3fff)]
            classes = [0, 1, 1, 2, 2, 2, 2, 2]
            output = (ctypes.c_uint8 * 108)()
            memory = bytearray(4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            cases, rendered_cases = 0, 0
            for masked in (False, True):
                for top in range(8):
                    for occupied in range(256):
                        initial = bytearray(108)
                        control, status = 0x37e | masked, 0x4700 | (top << 11)
                        tags = sum((3 if not (occupied & (1 << i)) else
                                    classes[i]) << (i*2)
                                   for i in range(8))
                        struct.pack_into('<H', initial, 0, control)
                        struct.pack_into('<H', initial, 4, status)
                        struct.pack_into('<H', initial, 8, tags)
                        struct.pack_into('<IHHIH', initial, 12, 0x123456, 16, 0x321, 0x234567, 16)
                        for logical in range(8):
                            struct.pack_into('<QH', initial, 28+logical*10, *values[(top+logical)&7])
                        native = (ctypes.c_uint8 * 108).from_buffer(initial)
                        for function, lifted_function, (source, operation, argument) in zip(
                                functions, lifted_functions, operations):
                            state = diff.State()
                            state.flags, state.pc = 0xad7, 0x123456
                            state.x87_control, state.x87_status, state.x87_tag = control, status, tags
                            for index, (significand, exponent) in enumerate(values):
                                state.x87[index][:] = [significand & 0xffffffff, significand >> 32, exponent]
                            saved = bytes(state)
                            function(native, output)
                            self.assertEqual(run(ctypes.byref(state), operation, argument), 0)
                            observed = bytes(output)
                            expected_status, expected_tags = struct.unpack_from('<H', observed, 4)[0], \
                                                             struct.unpack_from('<H', observed, 8)[0]
                            expected_top = (expected_status >> 11) & 7
                            expected = [struct.unpack_from('<QH', observed, 28+((i-expected_top)&7)*10)
                                        for i in range(8)]
                            actual = [(row[0] | (row[1] << 32), row[2]) for row in state.x87]
                            context = (source, masked, top, hex(tags))
                            self.assertEqual((state.x87_status, state.x87_tag),
                                             (expected_status, expected_tags), context)
                            self.assertEqual(actual, expected, context)
                            self.assertEqual((state.flags, state.pc, state.x87_control),
                                             (0xad7, 0x123456, control), context)
                            cases += 1
                            if occupied in (0, 255, 1 << top, 255 ^ (1 << top)):
                                rendered = diff.State.from_buffer_copy(saved)
                                rendered.pc, rendered.r[4], rendered.memory, rendered.memory_size = 0x100, 0x800, ram, len(memory)
                                rendered.segment[1] = 8
                                rendered.x87_ip, rendered.x87_cs, rendered.x87_opcode = 0x123456, 16, 0x321
                                rendered.x87_dp, rendered.x87_ds = 0x234567, 16
                                self.assertEqual(lifted_function(ctypes.byref(rendered), 10), 0, context)
                                self.assertEqual((rendered.x87_status, rendered.x87_tag),
                                                 (expected_status, expected_tags), context)
                                self.assertEqual([list(row) for row in rendered.x87],
                                                 [list(row) for row in state.x87], context)
                                # Relocate host instruction IP/CS into guest
                                # context; opcode and data environment compare
                                # directly with the independently executed ISA.
                                self.assertNotEqual(struct.unpack_from('<I', observed, 12)[0], 0x123456)
                                self.assertEqual((rendered.x87_ip, rendered.x87_cs), (0x100, 8), context)
                                self.assertEqual(rendered.x87_opcode,
                                                 struct.unpack_from('<H', observed, 18)[0] & 0x7ff, context)
                                self.assertEqual((rendered.x87_dp, rendered.x87_ds),
                                    (struct.unpack_from('<I', observed, 20)[0],
                                     struct.unpack_from('<H', observed, 24)[0]), context)
                                self.assertEqual((rendered.flags, rendered.pc, rendered.r[4]),
                                                 (0xad7, 0xf00, 0x804), context)
                                rendered_cases += 1
            self.assertEqual(cases, 188416)
            self.assertEqual(rendered_cases, 2944)
            # Waiting operations may not consume or silently clear pending #MF.
            state.x87_status = 0x80c1
            previous = bytes(state)
            self.assertEqual(run(ctypes.byref(state), 0, 0), 8)
            self.assertEqual(bytes(state), previous)
            rendered.pc, rendered.r[4], rendered.x87_status = 0x100, 0x800, 0x80c1
            previous = bytes(rendered)
            self.assertEqual(lifted_functions[0](ctypes.byref(rendered), 10), 8)
            self.assertEqual(bytes(rendered), previous)
