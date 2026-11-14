# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""FIST/FISTP rounding, exception priority, memory and retained stack payloads."""

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
class IntegerStoreTests(unittest.TestCase):
    def test_integer_stores_against_native(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native, report = oracle.build(root/'original')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            specifications = [(0xdf16, 16), (0xdf1e, 16), (0xdb16, 32), (0xdb1e, 32), (0xdf3e, 64)]
            codes = []
            for number, (opcode, _) in enumerate(specifications):
                _, record = assemble(root, '.byte %d,%d\nret' % (opcode >> 8, opcode & 255))
                codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d' % number))
            source = root/'lifted.c'
            source.write_text('\n'.join(codes))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(source), '-o', str(root/'lifted.so')], check=True)
            if os.environ.get('ASM_INTEGER_STORE_WCC'):
                subprocess.run([os.environ['ASM_INTEGER_STORE_WCC'], '-bt=dos', '-3r', '-za99', '-w1', '-we',
                                '-fo='+str(root/'lifted.obj'), str(source)], check=True, cwd=root)
            library = ctypes.CDLL(str(root/'lifted.so'))
            memory = bytearray(b'\xa5'*4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8*4096).from_buffer(memory)
            template = diff.State((ctypes.c_uint32*8)(), 0xad7, 0x100, 0, ram, 4096)
            template.r[4], template.r[6] = 0x800, 0x500
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_compare_profile = native.comparison_profile['flags']
            baseline = bytes(template)
            cases, interrupted = 0, 0
            for number, (opcode, width) in enumerate(specifications):
                function = getattr(library, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                # Halfway cases, range boundaries, denormals, unsupported encodings and NaNs.
                vectors = [(0, 0), (1 << 63, 0x3ffe), ((1 << 63)-1, 0x3ffe),
                           ((1 << 63)+1, 0x3ffe), (1 << 63, 0x3fff), (3 << 62, 0x3fff),
                           (5 << 61, 0x4000), (1, 0), ((1 << 63)-1, 0), (1 << 63, 0),
                           (1 << 63, 1), (1 << 63, 0x7ffe), (1 << 63, 0x7fff),
                           (0xc000000000000003, 0x7fff), (0x8000000000000003, 0x7fff),
                           (0, 0x7fff), (0x4000000000000000, 0x3fff),
                           (1 << 63, 0x3fff+width-1), ((1 << 63)+1, 0x3fff+width-1),
                           (0xffffffffffffffff, 0x3fff+width-2)]
                vectors = [(sig, exp | sign) for sig, exp in vectors for sign in (0, 0x8000)]
                for masks in range(64):
                    for precision in (0, 2, 3):
                        for rounding in range(4):
                            for vector, (sig, exp) in enumerate(vectors):
                                for empty in (False, True):
                                    top = (masks+precision+rounding+vector) & 7
                                    values = [[i, 0x80000000, 0x3fff] for i in range(8)]
                                    values[top] = [sig & 0xffffffff, sig >> 32, exp]
                                    tag = 1 if not sig and not (exp & 0x7fff) else (
                                        2 if (exp & 0x7fff) in (0, 0x7fff) or not sig >> 63 else 0)
                                    tags = (3 if empty else tag) << (top*2)
                                    control = 0x40 | masks | precision << 8 | rounding << 10
                                    status = 0x4700 | top << 11 | (masks & (vector*13 & 0x3f))
                                    snapshot = dict(x87_control=control, x87_status=status, x87_tag=tags,
                                                    x87_opcode=0x321, x87=values)
                                    data = b'\xa5'*(width//8)
                                    expected, written, _, pc, dp, retries = native.execute(opcode, snapshot, data, 0)
                                    interrupted += retries
                                    memory[0x4ff:0x509] = b'\xa5'*10
                                    state = diff.State.from_buffer_copy(baseline)
                                    diff.set_x87(state, dict(snapshot, x87_ip=0x123456, x87_cs=16,
                                                            x87_dp=0x234567, x87_ds=16))
                                    context = (hex(opcode), hex(control), top, vector, empty)
                                    self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                                    self.assertEqual((state.x87_status, state.x87_tag, state.x87_control),
                                                     (expected['x87_status'], expected['x87_tag'], expected['x87_control']), context)
                                    self.assertEqual([list(v) for v in state.x87], expected['x87'], context)
                                    self.assertEqual(state.x87_opcode, expected['x87_opcode'], context)
                                    self.assertEqual((expected['x87_ip'], expected['x87_cs']), (pc, native.host_cs), context)
                                    self.assertEqual((expected['x87_dp'], expected['x87_ds']), (dp, native.host_ds), context)
                                    self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                                     (0x100, 8, 0x500, 16), context)
                                    self.assertEqual(bytes(memory[0x4ff:0x509]), b'\xa5'+written+b'\xa5'*(9-len(written)), context)
                                    self.assertEqual((state.flags, state.pc, state.r[4], state.fault,
                                                      state.x87_compare_profile), (0xad7, 0xf00, 0x804, 0, template.x87_compare_profile))
                                    cases += 1
                for address in (4096-width//8+1, 4096, 0xffffffff):
                    state = diff.State.from_buffer_copy(baseline)
                    state.r[6] = address
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                    state.x87_status = 0x8081
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 8)
                    self.assertEqual(bytes(state), before)
            self.assertEqual(cases, 307200)
            if os.environ.get('ASM_INTEGER_STORE_EVIDENCE'):
                Path(os.environ['ASM_INTEGER_STORE_EVIDENCE']).write_text(json.dumps(dict(cases=cases,
                    interrupted_native_samples=interrupted, original=report,
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()), indent=2)+'\n')
