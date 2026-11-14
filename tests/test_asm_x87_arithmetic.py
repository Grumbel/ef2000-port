# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Generated x87 register arithmetic versus independent hardware execution."""

import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_softfloat, asm_c_differential as diff
from tests.test_asm_to_c import assemble
from tests.test_asm_c_program import inventory


@unittest.skipUnless(os.environ.get('ASM_SOFTFLOAT_BUILD') and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires x86-64, cc/binutils and ASM_SOFTFLOAT_BUILD')
class ArithmeticTests(unittest.TestCase):
    def test_register_results_masks_pop_and_environment(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            backend = asm_softfloat.build_binary(Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve(), root/'backend')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            _, plain = assemble(root, 'ret')
            codes = [asm_to_c.translate(plain).replace('ef_lift_00000100', 'plain_ret')]
            probes, specs = [], []
            for first in (0xd8, 0xdc, 0xde):
                for group in (0, 1, 4, 5, 6, 7):
                    for index in range(8):
                        number = len(specs)
                        second = 0xc0 | group << 3 | index
                        _, record = assemble(root, '.byte %d,%d\nret' % (first, second))
                        codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d' % number))
                        probes.append('''#include <stdint.h>
uintptr_t native%d(const void *initial,void *out) {
 unsigned char saved[108]; uintptr_t pc;
 __asm__ volatile("fnsave %%[save]\\n\\tfrstor (%%[initial])\\n\\t1: .byte %d,%d\\n\\t"
 "fnsave (%%[out])\\n\\tfrstor %%[save]\\n\\tleaq 1b(%%%%rip),%%[pc]"
 : [save] "+m"(saved),[pc] "=r"(pc) : [initial] "r"(initial),[out] "r"(out) : "memory"); return pc;
}''' % (number, first, second))
                        specs.append((first, second, index))
            (root/'native.c').write_text('\n'.join(probes))
            (root/'lifted.c').write_text('\n'.join(codes))
            for name in ('native', 'lifted'):
                command = ['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                           '-Wno-unused-function', str(root/(name+'.c')), '-o', str(root/(name+'.so'))]
                if name == 'lifted':
                    command += [str(root/'backend/binary.so'), '-Wl,-z,defs',
                                '-fsanitize=undefined', '-fno-sanitize-recover=all']
                subprocess.run(command, check=True)
            hardware, generated = [ctypes.CDLL(str(root/(name+'.so'))) for name in ('native', 'lifted')]
            memory = bytearray(b'\xa5'*4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8*4096).from_buffer(memory)
            template = diff.State((ctypes.c_uint32*8)(), 0xad7, 0x100, 0, ram, 4096)
            template.r[4] = 0x800
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_ip, template.x87_cs, template.x87_opcode = 0x123456, 16, 0x321
            template.x87_dp, template.x87_ds = 0x234567, 16
            base_values = [(0x8000000000000000+i, 0x3fff) for i in range(8)]
            baseline, captured = bytes(template), (ctypes.c_uint8*108)()
            one, zero = (1 << 63, 0x3fff), (0, 0)
            vectors = [(one, one, 0), (one, ((1 << 63)+1, 0x3fff), 0),
                       (((1 << 64)-1, 0x7ffe), (1 << 63, 0x4000), 0),
                       ((1 << 63, 1), (1 << 63, 0x3ffe), 0),
                       (((1 << 63)+1, 1), ((1 << 64)-1, 0x4000), 0),
                       ((1, 0), one, 0), ((1, 0), zero, 0), (one, zero, 0),
                       (zero, zero, 0), ((1 << 63, 0xbfff), one, 0),
                       ((1 << 63, 0), one, 0), ((0xc000000000000123, 0x7fff), (1, 0), 0),
                       ((0x8000000000000123, 0x7fff), one, 0),
                       ((0x4000000000000000, 0x3fff), one, 0),
                       (one, one, 1), (one, one, 2), (one, one, 3)]
            cases, interrupted_samples = 0, 0
            usage_scope = getattr(resource, 'RUSAGE_THREAD', resource.RUSAGE_SELF)

            def switches():
                usage = resource.getrusage(usage_scope)
                return usage.ru_nvcsw, usage.ru_nivcsw
            for number, (first, second, index) in enumerate(specs):
                native = getattr(hardware, 'native%d' % number)
                native.argtypes, native.restype = [ctypes.c_void_p]*2, ctypes.c_uint64
                function = getattr(generated, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                for masks in range(64):
                    for precision in (0, 2, 3):
                        for rounding in range(4):
                            control = 0x40 | masks | precision << 8 | rounding << 10
                            for vector, (a, b, empty) in enumerate(vectors):
                                top = (masks + precision + rounding + vector) & 7
                                other = (top+index) & 7
                                values = list(base_values)
                                values[top], values[other] = a, b
                                tags = 0
                                for i, (sig, exp) in enumerate(values):
                                    tag = 1 if not (exp & 0x7fff) and not sig else (
                                        2 if (exp & 0x7fff) in (0, 0x7fff) or not sig >> 63 else 0)
                                    if (i == top and empty & 1) or (i == other and empty & 2):
                                        tag = 3
                                    tags |= tag << (i*2)
                                status = 0x4700 | top << 11
                                # Seed sticky flags only when masked; never create
                                # an inconsistent pending-exception environment.
                                status |= masks & ((vector*13) & 0x3f)
                                initial = bytearray(108)
                                struct.pack_into('<H', initial, 0, control)
                                struct.pack_into('<H', initial, 4, status)
                                struct.pack_into('<H', initial, 8, tags)
                                struct.pack_into('<IHHIH', initial, 12, 0x123456, 16, 0x321, 0x234567, 16)
                                for logical in range(8):
                                    struct.pack_into('<QH', initial, 28+logical*10, *values[(top+logical)&7])
                                raw_initial = (ctypes.c_uint8*108).from_buffer(initial)
                                # AMD error pointers may be lost during an OS
                                # FPU context restore without pending #MF. Reject
                                # interrupted measurements before inspecting any
                                # result fields or running C; never waive a diff.
                                for attempt in range(100):
                                    before_switches = switches()
                                    pc = native(raw_initial, captured)
                                    if switches() == before_switches:
                                        break
                                    interrupted_samples += 1
                                else:
                                    self.fail('no uninterrupted native x87 sample')
                                state = diff.State.from_buffer_copy(baseline)
                                state.x87_control, state.x87_status, state.x87_tag = control, status, tags
                                for i, (sig, exp) in enumerate(values):
                                    state.x87[i][:] = [sig & 0xffffffff, sig >> 32, exp]
                                context = (hex(first), hex(second), index, hex(control), top, vector)
                                self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                                observed = bytes(captured)
                                sw = struct.unpack_from('<H', observed, 4)[0]
                                expected_top = (sw >> 11) & 7
                                self.assertEqual((state.x87_status, state.x87_tag),
                                                 (sw, struct.unpack_from('<H', observed, 8)[0]), context)
                                self.assertEqual([(r[0] | r[1] << 32, r[2]) for r in state.x87],
                                    [struct.unpack_from('<QH', observed, 28+((i-expected_top)&7)*10)
                                     for i in range(8)], context)
                                self.assertEqual(struct.unpack_from('<I', observed, 12)[0], pc & 0xffffffff)
                                self.assertEqual(state.x87_opcode, struct.unpack_from('<H', observed, 18)[0] & 0x7ff)
                                self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                                 (0x100, 8, 0x234567, 16))
                                self.assertEqual((state.x87_dp, state.x87_ds),
                                                 (struct.unpack_from('<I', observed, 20)[0],
                                                  struct.unpack_from('<H', observed, 24)[0]), context)
                                self.assertEqual(state.x87_control, struct.unpack_from('<H', observed, 0)[0])
                                self.assertEqual((state.flags, state.pc, state.r[4]), (0xad7, 0xf00, 0x804))
                                self.assertEqual(list(state.r), [0, 0, 0, 0, 0x804, 0, 0, 0])
                                self.assertEqual(list(state.segment), [16, 8, 24, 16, 16, 16])
                                self.assertEqual(state.fault, 0)
                                cases += 1
                state = diff.State.from_buffer_copy(baseline)
                state.x87_status = 0x8081
                before = bytes(state)
                self.assertEqual(function(ctypes.byref(state), 10), 8)
                self.assertEqual(bytes(state), before)
            self.assertEqual(bytes(memory), b'\xa5'*0x800 + (0xf00).to_bytes(4, 'little') + b'\xa5'*(4096-0x804))
            self.assertEqual(cases, 1880064)
            if os.environ.get('ASM_ARITHMETIC_EVIDENCE'):
                Path(os.environ['ASM_ARITHMETIC_EVIDENCE']).write_text(json.dumps(dict(
                    generated_cases=cases, forms=len(specs),
                    interrupted_native_samples=interrupted_samples,
                    adapter_source_sha256=backend['source_sha256'],
                    dependency_library_sha256=backend['dependency_library_sha256'],
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    generated_source_sha256=hashlib.sha256((root/'lifted.c').read_bytes()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    scope='Register arithmetic, all masks/PC/RC, raw state and relocated environment; no #MF delivery.'),
                    indent=2) + '\n')

    def test_dependency_binding_and_encoding_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, inv, _ = inventory(root, '.type root,@function\nroot: fdiv st,st(1)\nret\n.size root,.-root')
            out = root/'out'
            report = asm_to_c.generate(inv, out, image)
            with self.assertRaisesRegex(ValueError, 'require --softfloat-build'):
                diff.verify(out, root/'evidence', image)
            self.assertFalse((root/'evidence').exists())
            for field, value, message in [('required_backends', [], 'backend declaration'),
                                         ('binary_backend_source_sha256', '0'*64, 'adapter source digest')]:
                altered = dict(report)
                altered[field] = value
                (out/'report.json').write_text(json.dumps(altered))
                with self.assertRaisesRegex(ValueError, message):
                    diff.verify(out, root/'evidence', image, compiler='must-not-run',
                                softfloat_build=Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve())
                self.assertFalse((root/'evidence').exists())
            _, record = assemble(root, 'fsub st,st(1)\nret')
            for field, value in [('bytes', 'd8e9'), ('operands', 'st(1),st')]:
                altered = json.loads(json.dumps(record))
                altered['instructions'][0][field] = value
                with self.assertRaises(asm_to_c.Unsupported):
                    asm_to_c.translate(altered)
