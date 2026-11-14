# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Raw comparison condition codes, masks, NaNs, retained payloads and pops."""

import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff, asm_x87_oracle as oracle
from tests.test_asm_to_c import assemble, unicorn


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Linux x86-64, binutils and cc')
class CompareTests(unittest.TestCase):
    def test_register_and_memory_state_against_native(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native, report = oracle.build(root/'original')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            specifications, codes = [], []
            for opcode, base, quiet, pop in ((0xd8, 0xd0, 0, 0), (0xd8, 0xd8, 0, 1),
                                              (0xdd, 0xe0, 1, 0), (0xdd, 0xe8, 1, 1)):
                for index in range(8):
                    specifications.append((opcode << 8 | base+index, index, 0, quiet, pop))
            specifications += [(0xded9, 1, 0, 0, 2), (0xdae9, 1, 0, 1, 2), (0xd9e4, 8, 0, 0, 0)]
            for opcode, width in ((0xd8, 32), (0xdc, 64)):
                for group in (2, 3):
                    specifications.append((opcode << 8 | group << 3 | 6, 8, width, 0, group-2))
            for number, (opcode, *_) in enumerate(specifications):
                _, record = assemble(root, '.byte %d,%d\nret' % (opcode >> 8, opcode & 255))
                codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d' % number))
            (root/'lifted.c').write_text('\n'.join(codes))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(root/'lifted.c'), '-o', str(root/'lifted.so')], check=True)
            library = ctypes.CDLL(str(root/'lifted.so'))
            memory = bytearray(b'\xa5'*4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8*4096).from_buffer(memory)
            template = diff.State((ctypes.c_uint32*8)(), 0xad7, 0x100, 0, ram, 4096)
            template.r[4], template.r[6] = 0x800, 0x500
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_ip, template.x87_cs, template.x87_opcode = 0x123456, 16, 0x321
            template.x87_dp, template.x87_ds = 0x234567, 16
            template.x87_compare_profile = native.comparison_profile['flags']
            baseline = bytes(template)
            one, two, zero = (1 << 63, 0x3fff), (1 << 63, 0x4000), (0, 0)
            qnan, snan, den = (0xc000000000000003, 0x7fff), (0x8000000000000003, 0x7fff), (1, 0)
            vectors = [(one, two), (two, one), (one, one), (zero, (0, 0x8000)),
                       ((1 << 63, 0xbfff), one), (den, zero), (den, one), (one, den),
                       ((1 << 63, 0), (1 << 63, 1)), ((0x7fffffffffffffff, 0), (1 << 63, 1)),
                       (qnan, one), (snan, one), ((0x4000000000000000, 0x3fff), qnan),
                       (qnan, den), (snan, den), ((1 << 63, 0x7fff), one),
                       ((1 << 63, 0xffff), (1 << 63, 0x7fff)),
                       ((1 << 63, 0xbfff), (1 << 63, 0xc000)),
                       ((1 << 63, 0xc000), (1 << 63, 0xbfff)),
                       ((0, 0x8000), (1, 0x8000)), ((1 << 63, 0x8001), (1, 0x8000))]
            cases, interrupted = 0, 0
            for number, (opcode, index, width, quiet, pop) in enumerate(specifications):
                function = getattr(library, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                current_vectors = vectors
                if width:
                    fraction, bias = (23, 127) if width == 32 else (52, 1023)
                    real_one = bias << fraction
                    inf = ((1 << (8 if width == 32 else 11))-1) << fraction
                    current_vectors = [(one, real_one), (two, real_one), (zero, 0), (zero, 1 << (width-1)),
                                       (den, real_one), (zero, 1), (qnan, 1), (snan, 1),
                                       (one, inf | 3), (one, inf | 1 << (fraction-1) | 3),
                                       ((0x4000000000000000, 0x3fff), real_one), (one, inf),
                                       ((1 << 63, 0xbfff), real_one | 1 << (width-1)),
                                       ((1 << 63, 0xc000), real_one | 1 << (width-1)),
                                       ((1 << 63, 0xbfff), (real_one+(1 << fraction)) | 1 << (width-1)),
                                       ((0, 0x8000), (1 << (width-1)) | 1)]
                for masks in range(64):
                    for precision in (0, 2, 3):
                        for rounding in range(4):
                            for vector, (a, b) in enumerate(current_vectors):
                                for empty in range(4 if not width else 2):
                                    top = (masks+precision+rounding+vector) & 7
                                    other = (top+index) & 7
                                    values = [(0x8000000000000000+i, 0x3fff) for i in range(8)]
                                    values[top] = a
                                    if index < 8:
                                        values[other] = b
                                    tags = 0
                                    for i, (sig, exp) in enumerate(values):
                                        tag = 1 if not (exp & 0x7fff) and not sig else (
                                            2 if (exp & 0x7fff) in (0, 0x7fff) or not sig >> 63 else 0)
                                        if (i == top and empty & 1) or (index < 8 and i == other and empty & 2): tag = 3
                                        tags |= tag << (2*i)
                                    control = 0x40 | masks | precision << 8 | rounding << 10
                                    status = ((vector & 1)*0x100 | ((vector >> 1) & 1)*0x400 |
                                              ((vector >> 2) & 1)*0x4000 | 0x200 | top << 11 |
                                              masks & ((vector*13) & 0x3f))
                                    snapshot = dict(x87_control=control, x87_status=status, x87_tag=tags,
                                                    x87_opcode=0x321, x87=[[s & 0xffffffff, s >> 32, e] for s, e in values])
                                    data = b.to_bytes(width//8, 'little') if width else b''
                                    expected, written, ax, pc, dp, retries = native.execute(opcode, snapshot, data, 17)
                                    interrupted += retries
                                    if width: memory[0x500:0x500+width//8] = data
                                    state = diff.State.from_buffer_copy(baseline)
                                    diff.set_x87(state, dict(snapshot, x87_ip=0x123456, x87_cs=16, x87_dp=0x234567, x87_ds=16))
                                    context = (hex(opcode), hex(control), top, vector, empty)
                                    self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                                    self.assertEqual((state.x87_status, state.x87_tag, state.x87_control),
                                                     (expected['x87_status'], expected['x87_tag'], expected['x87_control']), context)
                                    self.assertEqual([list(v) for v in state.x87], expected['x87'], context)
                                    self.assertEqual(state.x87_opcode, expected['x87_opcode'], context)
                                    self.assertEqual((expected['x87_ip'], expected['x87_cs']), (pc, native.host_cs), context)
                                    self.assertEqual((state.x87_ip, state.x87_cs), (0x100, 8))
                                    self.assertEqual((state.x87_dp, state.x87_ds), (0x500, 16) if width else (0x234567, 16))
                                    if width:
                                        self.assertEqual((expected['x87_dp'], expected['x87_ds']), (dp, native.host_ds))
                                        self.assertEqual(bytes(memory[0x500:0x500+width//8]), data)
                                    else:
                                        self.assertEqual((expected['x87_dp'], expected['x87_ds']), (0xdecaf002, 0x5678))
                                    self.assertEqual(written, data)
                                    self.assertEqual((state.flags, state.pc, state.r[4], state.fault), (0xad7, 0xf00, 0x804, 0))
                                    cases += 1
                if width:
                    state = diff.State.from_buffer_copy(baseline)
                    state.r[6] = 4096-width//8+1
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                state = diff.State.from_buffer_copy(baseline)
                state.x87_status = 0x8081
                before = bytes(state)
                self.assertEqual(function(ctypes.byref(state), 10), 8)
                self.assertEqual(bytes(state), before)
            self.assertEqual(cases, 2356224)
            if os.environ.get('ASM_COMPARE_EVIDENCE'):
                Path(os.environ['ASM_COMPARE_EVIDENCE']).write_text(json.dumps(dict(cases=cases,
                    interrupted_native_samples=interrupted, original=report,
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()), indent=2)+'\n')

    def test_explicit_unmasked_profile_and_unsupported_profile(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            _, record = assemble(root, 'fcomp st(1)\nret')
            (root/'lifted.c').write_text(asm_to_c.translate(record))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wno-unused-function', str(root/'lifted.c'),
                            '-o', str(root/'lifted.so')], check=True)
            function = ctypes.CDLL(str(root/'lifted.so')).ef_lift_00000100
            function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
            ram = (ctypes.c_uint8*4096)()
            for profile in range(5):
                for denormal in (False, True):
                    state = diff.State((ctypes.c_uint32*8)(), 0x202, 0x100, 0, ram, 4096)
                    state.segment[:] = [16, 8, 16, 16, 16, 16]
                    state.x87_compare_profile, state.x87_control, state.x87_tag = profile, 0x37d if denormal else 0x37e, 0xfff2
                    state.x87_status = 0x4000 if denormal else 0
                    state.x87[0][:] = [1, 0, 0] if denormal else [1, 0xc0000000, 0x7fff]
                    state.x87[1][:] = [0, 0x80000000, 0x3fff]
                    if profile == 4:
                        before = bytes(state)
                        self.assertEqual(function(ctypes.byref(state), 1), 9)
                        self.assertEqual(bytes(state), before)
                    else:
                        self.assertEqual(function(ctypes.byref(state), 1), 1)
                        expected = (0x8182 if profile & 2 else 0xc082) if denormal else (0xc581 if profile & 1 else 0x8081)
                        self.assertEqual(state.x87_status, expected)
                        self.assertEqual(state.x87_tag, 0xfff2)
