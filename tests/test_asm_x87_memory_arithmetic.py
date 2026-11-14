# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Full memory arithmetic effects against independent native instructions."""

import ctypes
import hashlib
import json
import multiprocessing
import os
from pathlib import Path
import platform
import resource
import shutil
import signal
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_softfloat, asm_x87_oracle, asm_c_differential as diff
from tests.test_asm_to_c import assemble, unicorn


@unittest.skipUnless(os.environ.get('ASM_SOFTFLOAT_BUILD') and platform.system() == 'Linux'
                     and platform.machine() == 'x86_64' and all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires Linux x86-64, cc/binutils and ASM_SOFTFLOAT_BUILD')
class MemoryArithmeticTests(unittest.TestCase):
    @unittest.skipUnless(unicorn, 'requires Unicorn')
    def test_addressing_and_segment_metadata_in_composed_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asm_softfloat.build_binary(Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve(), root/'backend')
            native, _ = asm_x87_oracle.build(root/'original')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            codes, payloads = [], []
            addresses = ('[eax]', '[ebp-4]', '[esp+4]', '[eax+ecx*4-4]', '[ecx*4+0x300100]',
                         'es:[esi]', 'fs:[edi]', 'gs:[eax+16]', 'ss:[0x300100]', '[0x300100]')
            for width in ('dword', 'qword'):
                for mnemonic in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
                    for address in addresses:
                        image, record = assemble(root, 'mov bx,24\nmov ss,bx\nmov es,bx\nmov fs,bx\nmov gs,bx\n'
                                                 'fld1\n'+mnemonic+' '+width+' ptr '+address+'\nret')
                        codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d' % len(codes)))
                        payloads.append(image.read_bytes())
            (root/'addressing.c').write_text('\n'.join(codes))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', str(root/'addressing.c'), str(root/'backend/binary.so'),
                            '-Wl,-z,defs', '-o', str(root/'addressing.so')], check=True)
            library = ctypes.CDLL(str(root/'addressing.so'))
            for number, payload in enumerate(payloads):
                function = getattr(library, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                result = diff.compare(function, payload, 0x100,
                                      [0x300100, 4, 0x300700, 0x300400, 0x3f0000, 0x300500, 0x300200, 0x300600],
                                      0xad7, native_x87=native)
                self.assertEqual(result['state'], 'pass', (number, result))
            if os.environ.get('ASM_MEMORY_ARITHMETIC_WATCOM'):
                command = [os.environ['ASM_MEMORY_ARITHMETIC_WATCOM'], '-q', '-bt=dos', '-3r', '-za99', '-w1', '-we',
                           '-i='+str(root), '-fo='+str(root/'addressing.obj'), str(root/'addressing.c')]
                compiled = subprocess.run(command, capture_output=True, text=True)
                compiled.check_returncode()
                self.assertEqual(compiled.stdout+compiled.stderr, '')

    def test_generated_values_masks_priority_and_environment(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            backend = asm_softfloat.build_binary(Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve(), root/'backend')
            native, native_report = asm_x87_oracle.build(root/'original')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            _, plain = assemble(root, 'ret')
            codes = [asm_to_c.translate(plain).replace('ef_lift_00000100', 'plain_ret')]
            specs = []
            for width in (32, 64):
                for mnemonic in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
                    _, record = assemble(root, mnemonic+' '+('dword' if width == 32 else 'qword')+' ptr [esi]\nret')
                    codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d' % len(specs)))
                    specs.append((width, mnemonic, int(record['instructions'][0]['bytes'], 16)))
            (root/'lifted.c').write_text('\n'.join(codes))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(root/'lifted.c'), str(root/'backend/binary.so'), '-Wl,-z,defs',
                            '-o', str(root/'lifted.so')], check=True)
            generated = ctypes.CDLL(str(root/'lifted.so'))
            memory = bytearray(b'\xa5'*4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8*4096).from_buffer(memory)
            template = diff.State((ctypes.c_uint32*8)(), 0xad7, 0x100, 0, ram, 4096)
            template.r[4], template.r[6] = 0x800, 0x500
            template.segment[:] = [16, 8, 24, 16, 16, 16]
            template.x87_ip, template.x87_cs, template.x87_opcode = 0x123456, 16, 0x321
            template.x87_dp, template.x87_ds = 0x234567, 16
            base = [(0x8000000000000000+i, 0x3fff) for i in range(8)]
            baseline, cases, interruptions = bytes(template), 0, 0
            for number, (width, mnemonic, opcode) in enumerate(specs):
                function = getattr(generated, 'op%d' % number)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                fraction_bits, bias = (23, 127) if width == 32 else (52, 1023)
                one = bias << fraction_bits
                infinity = ((1 << (8 if width == 32 else 11))-1) << fraction_bits
                qnan, snan = infinity | 1 << (fraction_bits-1) | 3, infinity | 3
                normal = (1 << 63, 0x3fff)
                vectors = [(normal, one), (normal, one | 1), (normal, one | 1 << (fraction_bits-1)),
                           (((1 << 64)-1, 0x7ffe), one+ (1 << fraction_bits)),
                           ((1 << 63, 1), one- (1 << fraction_bits)),
                           (((1 << 63)+1, 1), one | 1 << (fraction_bits-1)),
                           (normal, 1), (((1 << 64)-1, 0x7ffe), 1), ((1 << 63, 1), 1),
                           ((0, 0), 1), ((1, 0), one), ((1 << 63, 0), one),
                           ((0, 0), 0), (normal, 0), ((1 << 63, 0x7fff), 0),
                           ((0x8000000000000003, 0x7fff), 1),
                           ((0xc000000000000003, 0x7fff), 1),
                           ((0x4000000000000000, 0x3fff), qnan),
                           (normal, qnan), (normal, snan),
                           ((0x8000000000000003, 0x7fff), qnan),
                           ((0xc000000000000003, 0x7fff), snan), (normal, infinity)]
                vectors += [((sig, exp ^ 0x8000), bits ^ (1 << (width-1))) for (sig, exp), bits in vectors]
                for masks in range(64):
                    for precision in (0, 2, 3):
                        for rounding in range(4):
                            control = 0x40 | masks | precision << 8 | rounding << 10
                            for vector, (value, bits) in enumerate(vectors):
                                top = (masks+precision+rounding+vector) & 7
                                for empty in (False, True):
                                    current = list(base)
                                    current[top] = value
                                    sig, exp = value
                                    tag = 3 if empty else 1 if not (exp & 0x7fff) and not sig else (
                                        2 if (exp & 0x7fff) in (0, 0x7fff) or not sig >> 63 else 0)
                                    tags = tag << (top*2)
                                    status = 0x4700 | top << 11 | (masks & ((vector*13) & 0x3f))
                                    values = dict(x87_control=control, x87_status=status, x87_tag=tags,
                                                  x87_opcode=0x321, x87=[[s & 0xffffffff, s >> 32, e] for s, e in current])
                                    data = bits.to_bytes(width//8, 'little')
                                    observed, written, ax, pc, dp, retry = native.execute(opcode, values, data, 17)
                                    interruptions += retry
                                    memory[0x500:0x508] = b'\xa5'*8
                                    memory[0x500:0x500+width//8] = data
                                    state = diff.State.from_buffer_copy(baseline)
                                    state.x87_control, state.x87_status, state.x87_tag = control, status, tags
                                    for i, words in enumerate(values['x87']):
                                        state.x87[i][:] = words
                                    context = (width, mnemonic, hex(control), top, vector, empty)
                                    self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                                    self.assertEqual((state.x87_status, state.x87_tag, state.x87_control),
                                                     (observed['x87_status'], observed['x87_tag'], observed['x87_control']), context)
                                    self.assertEqual([list(v) for v in state.x87], observed['x87'], context)
                                    self.assertEqual(state.x87_opcode, observed['x87_opcode'], context)
                                    self.assertEqual((observed['x87_ip'], observed['x87_cs'], observed['x87_dp'], observed['x87_ds']),
                                                     (pc, native.host_cs, dp, native.host_ds), context)
                                    self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds), (0x100, 8, 0x500, 16))
                                    self.assertEqual(written, data)
                                    self.assertEqual(bytes(memory[0x500:0x500+width//8]), data)
                                    self.assertEqual((memory[0x4ff], memory[0x508]), (0xa5, 0xa5))
                                    self.assertEqual((state.flags, state.pc, state.fault), (0xad7, 0xf00, 0))
                                    self.assertEqual(list(state.r), [0, 0, 0, 0, 0x804, 0, 0x500, 0])
                                    cases += 1
                for address, size in ((4096-width//8+1, 4096), (0, width//8-1), (0xffffffff, 4096)):
                    state = diff.State.from_buffer_copy(baseline)
                    state.r[6], state.memory_size = address, size
                    before, previous_ram = bytes(state), bytes(memory)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                    self.assertEqual(bytes(memory), previous_ram)
                    state.x87_status = 0x8081
                    before = bytes(state)
                    self.assertEqual(function(ctypes.byref(state), 10), 8)
                    self.assertEqual(bytes(state), before)
                # Actual CPU fault priority, deliberately bypassing safe operand
                # validation in a disposable child to execute a null address.
                for pending in (False, True):
                    def fault():
                        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                        values = dict(x87_control=0x37e, x87_status=0x8081 if pending else 0,
                                      x87_tag=0xffff, x87_opcode=0, x87=[[0, 0, 0] for _ in range(8)])
                        initial = (ctypes.c_uint8*108).from_buffer(asm_x87_oracle.pack(values))
                        native.run(opcode, initial, (ctypes.c_uint8*108)(), None, ctypes.byref(ctypes.c_uint32()))
                    process = multiprocessing.get_context('fork').Process(target=fault)
                    process.start(); process.join()
                    self.assertEqual(process.exitcode, -signal.SIGFPE if pending else -signal.SIGSEGV)
            self.assertEqual(cases, 847872)
            if os.environ.get('ASM_MEMORY_ARITHMETIC_EVIDENCE'):
                Path(os.environ['ASM_MEMORY_ARITHMETIC_EVIDENCE']).write_text(json.dumps(dict(
                    cases=cases, interrupted_native_samples=interruptions,
                    adapter=backend['source_sha256'], original=native_report,
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()), indent=2)+'\n')
