# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""PUSHA/POPA register ordering, skipped ESP, flags and flat-oracle faults."""

import ctypes
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff
from tests.test_asm_to_c import assemble, unicorn


@unittest.skipUnless(unicorn and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Unicorn, binutils and cc')
class StackAllTests(unittest.TestCase):
    def test_success_and_partial_faults_against_unicorn(self):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError, UC_HOOK_MEM_READ
        from unicorn import x86_const as x
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            codes = []
            for mnemonic in ('pusha', 'popa'):
                _, record = assemble(root, mnemonic+'\nret')
                codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', mnemonic))
            source = root/'lifted.c'
            source.write_text('\n'.join(codes))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(source), '-o', str(root/'lifted.so')], check=True)
            if os.environ.get('ASM_STACK_ALL_WCC'):
                subprocess.run([os.environ['ASM_STACK_ALL_WCC'], '-q', '-bt=dos', '-3r', '-za99', '-w1', '-we',
                                '-fo='+str(root/'lifted.obj'), str(source)], check=True, cwd=root)
            library = ctypes.CDLL(str(root/'lifted.so'))
            rng = random.Random(0x606187)
            size = 0x4000
            memory = bytearray(size)
            ram = (ctypes.c_uint8*size).from_buffer(memory)
            ids = [getattr(x, 'UC_X86_REG_'+reg.upper()) for reg in asm_to_c.REGS]
            counts = dict(success=0, fault=0)
            for number, mnemonic in enumerate(('pusha', 'popa')):
                function = getattr(library, mnemonic)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                stacks = list(range(0, 40))+list(range(size-40, size+40))
                stacks += [0xffffffdf, 0xffffffe0, 0xfffffffc, 0xffffffff]
                stacks += [0x800+rng.randrange(0x3000) for _ in range(512)]
                for case, stack in enumerate(stacks):
                    original = bytearray(rng.randbytes(size))
                    original[0x100:0x102] = bytes((0x60+number, 0xc3))
                    memory[:] = original
                    registers = [rng.getrandbits(32) for _ in range(8)]
                    registers[4] = stack
                    flags = 0x202 | (rng.getrandbits(12) & 0xcd5)
                    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
                    cpu.mem_map(0, size)
                    cpu.mem_write(0, bytes(original))
                    selectors = diff.bind_flat_segments(cpu)
                    for reg, value in zip(ids, registers): cpu.reg_write(reg, value)
                    cpu.reg_write(x.UC_X86_REG_EFLAGS, flags)
                    reads = []
                    cpu.hook_add(UC_HOOK_MEM_READ, lambda uc, access, address, width, value, data:
                                 reads.append((address, width)))
                    failed = False
                    try: cpu.emu_start(0x100, 0x101, count=1)
                    except UcError: failed = True
                    state = diff.State((ctypes.c_uint32*8)(*registers), flags, 0x100, 0, ram, size)
                    state.segment[:] = selectors
                    state.x87_control, state.x87_status, state.x87_tag = 0x37f, 0x4500, 0xffff
                    state.x87_ip, state.x87_cs, state.x87_opcode = 0x123456, 16, 0x321
                    state.x87_dp, state.x87_ds, state.x87_compare_profile = 0x234567, 16, 3
                    for i in range(8): state.x87[i][:] = [i, 0x80000000, 0x3fff]
                    before = bytes(state)
                    status = function(ctypes.byref(state), 1)
                    context = (mnemonic, case, hex(stack))
                    self.assertEqual(status, 3 if failed else 1, context)
                    self.assertEqual(state.fault, 3 if failed else 0, context)
                    self.assertEqual(list(state.r), [cpu.reg_read(reg) for reg in ids], context)
                    self.assertEqual(state.pc, cpu.reg_read(x.UC_X86_REG_EIP), context)
                    self.assertEqual(state.flags, cpu.reg_read(x.UC_X86_REG_EFLAGS), context)
                    self.assertEqual(bytes(memory), bytes(cpu.mem_read(0, size)), context)
                    # Everything outside GPRs/PC/fault must remain untouched.
                    expected = diff.State.from_buffer_copy(before)
                    expected.r[:] = state.r[:]
                    expected.pc, expected.fault = state.pc, state.fault
                    self.assertEqual(bytes(state), bytes(expected), context)
                    if not failed and mnemonic == 'popa':
                        self.assertEqual(reads, [(stack+offset, 4) for offset in (0, 4, 8, 16, 20, 24, 28)])
                    counts['fault' if failed else 'success'] += 1
            self.assertEqual(sum(counts.values()), 1272)
            if os.environ.get('ASM_STACK_ALL_EVIDENCE'):
                Path(os.environ['ASM_STACK_ALL_EVIDENCE']).write_text(json.dumps(dict(counts=counts,
                    oracle='Unicorn '+unicorn.__version__, runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    limits='Flat Unicorn state/memory observations, not native exception-delivery proof.'), indent=2)+'\n')

    def test_encoding_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            for mnemonic, wrong in (('pusha', '61'), ('popa', '60')):
                _, record = assemble(Path(temporary), mnemonic+'\nret')
                for encoded, operands in ((wrong, ''), ('6660', ''), ('f360', ''), ('60', 'eax')):
                    record['instructions'][0].update(bytes=encoded, operands=operands)
                    with self.assertRaises(asm_to_c.Unsupported): asm_to_c.translate(record)
