# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""FRNDINT raw payloads, rounding, exception masks and unrestricted range."""

import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff, asm_x87_oracle as oracle
from tests.test_asm_to_c import assemble


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Linux x86-64, binutils and cc')
class RoundTests(unittest.TestCase):
    def test_round_integer_against_native(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native, report = oracle.build(root/'original')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            _, record = assemble(root, 'frndint\nret')
            source = root/'lifted.c'
            source.write_text(asm_to_c.translate(record))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(source), '-o', str(root/'lifted.so')], check=True)
            if os.environ.get('ASM_ROUND_WCC'):
                subprocess.run([os.environ['ASM_ROUND_WCC'], '-q', '-bt=dos', '-3r', '-za99', '-w1', '-we',
                                '-fo='+str(root/'lifted.obj'), str(source)], check=True, cwd=root)
            library = ctypes.CDLL(str(root/'lifted.so'))
            function = library.ef_lift_00000100
            function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
            memory = bytearray(b'\xa5'*4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8*4096).from_buffer(memory)
            template = diff.State((ctypes.c_uint32*8)(*range(101, 109)), 0xad7, 0x100, 0, ram, 4096)
            template.r[4] = 0x800
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_compare_profile = native.comparison_profile['flags']
            baseline, memory_before = bytes(template), bytes(memory)
            registers = list(template.r)
            registers[4] += 4
            cases, interrupted = 0, 0

            def check(sig, exp, masks, precision, rounding, empty, vector):
                nonlocal cases, interrupted
                top = (masks+precision+rounding+vector) & 7
                values = [[i+1, 0x80000000, 0x3fff] for i in range(8)]
                values[top] = [sig & 0xffffffff, sig >> 32, exp]
                tag = 1 if not sig and not (exp & 0x7fff) else (
                    2 if (exp & 0x7fff) in (0, 0x7fff) or not sig >> 63 else 0)
                tags = (3 if empty else tag) << (top*2)
                control = 0x40 | masks | precision << 8 | rounding << 10
                status = ((vector & 1)*0x100 | ((vector >> 1) & 1)*0x400 |
                          ((vector >> 2) & 1)*0x4000 | 0x200 | top << 11 |
                          (masks & (vector*13 & 0x3f)))
                snapshot = dict(x87_control=control, x87_status=status, x87_tag=tags,
                                x87_opcode=0x321, x87=values)
                expected, written, ax, pc, dp, retries = native.execute(0xd9fc, snapshot, b'', 17)
                interrupted += retries
                state = diff.State.from_buffer_copy(baseline)
                diff.set_x87(state, dict(snapshot, x87_ip=0x123456, x87_cs=16,
                                        x87_dp=0x234567, x87_ds=16))
                context = (hex(sig), hex(exp), hex(control), top, empty)
                self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                self.assertEqual((state.x87_status, state.x87_tag, state.x87_control),
                                 (expected['x87_status'], expected['x87_tag'], expected['x87_control']), context)
                self.assertEqual([list(v) for v in state.x87], expected['x87'], context)
                self.assertEqual(state.x87_opcode, expected['x87_opcode'], context)
                self.assertEqual((expected['x87_ip'], expected['x87_cs']), (pc, native.host_cs), context)
                self.assertEqual((expected['x87_dp'], expected['x87_ds']), (0xdecaf002, 0x5678), context)
                self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                 (0x100, 8, 0x234567, 16), context)
                self.assertEqual((state.flags, state.pc, state.fault, state.x87_compare_profile),
                                 (0xad7, 0xf00, 0, template.x87_compare_profile), context)
                self.assertEqual(list(state.r), registers, context)
                self.assertEqual(list(state.segment), list(template.segment), context)
                self.assertEqual(bytes(memory), memory_before, context)
                self.assertEqual((written, ax), (b'', 17), context)
                cases += 1

            vectors = [(sig, exp) for exp in (0, 1, 0x3ffd, 0x3ffe, 0x3fff, 0x4000,
                        0x401d, 0x401e, 0x4033, 0x403d, 0x403e, 0x403f, 0x7ffe)
                       for sig in (1 << 63, (1 << 63)+1, 0xa000000000000000,
                                   0xc000000000000000, 0xffffffffffffffff)]
            vectors += [(0, 0), (1, 0), ((1 << 63)-1, 0), (0, 1), (1, 1),
                        (0, 0x3fff), (1 << 62, 0x3fff), (1 << 63, 0x7fff),
                        (0xc000000000000003, 0x7fff), (0x8000000000000003, 0x7fff),
                        (0, 0x7fff), (1 << 62, 0x7fff)]
            vectors = [(sig, exp | sign) for sig, exp in vectors for sign in (0, 0x8000)]
            for masks in range(64):
                for precision in (0, 2, 3):
                    for rounding in range(4):
                        for vector, (sig, exp) in enumerate(vectors):
                            for empty in (False, True):
                                check(sig, exp, masks, precision, rounding, empty, vector)
            matrix_cases = cases
            # Every exponent, both signs and all rounding modes: nontrivial raw
            # significands cover the integral/fraction boundary without host FP.
            for exponent in range(0x8000):
                sig = ((exponent*0x9e3779b97f4a7c15) & 0xffffffffffffffff) | (1 << 63)
                for sign in (0, 0x8000):
                    for rounding in range(4):
                        check(sig, exponent | sign, 63, (0, 2, 3)[exponent % 3], rounding, False, exponent)
            self.assertEqual(matrix_cases, 236544)
            self.assertEqual(cases, 498688)
            state = diff.State.from_buffer_copy(baseline)
            state.x87_status = 0x8081
            before = bytes(state)
            self.assertEqual(function(ctypes.byref(state), 10), 8)
            self.assertEqual(bytes(state), before)
            state = diff.State.from_buffer_copy(baseline)
            before = bytes(state)
            self.assertEqual(function(ctypes.byref(state), 0), 1)
            self.assertEqual(bytes(state), before)
            if os.environ.get('ASM_ROUND_EVIDENCE'):
                Path(os.environ['ASM_ROUND_EVIDENCE']).write_text(json.dumps(dict(cases=cases,
                    mask_matrix_cases=matrix_cases, exponent_sweep_cases=cases-matrix_cases,
                    interrupted_native_samples=interrupted, original=report,
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()), indent=2)+'\n')

    def test_reject_mismatched_encoding(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, record = assemble(Path(temporary), 'frndint\nret')
            for encoded, operands in (('d9fd', ''), ('66d9fc', ''), ('d9fc', 'st')):
                record['instructions'][0].update(bytes=encoded, operands=operands)
                with self.assertRaises(asm_to_c.Unsupported):
                    asm_to_c.translate(record)
