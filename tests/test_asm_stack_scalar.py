# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Scalar stack width, ESP-dependent addressing, aliasing and partial faults."""

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
class ScalarStackTests(unittest.TestCase):
    def test_width_addressing_and_faults_against_unicorn(self):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError
        from unicorn import x86_const as x
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            sources = [mnemonic+' '+register for mnemonic in ('push', 'pop')
                       for register in list(asm_to_c.REGS)+['ax','cx','dx','bx','sp','bp','si','di']]
            addresses = ['[esp]', '[esp-2]', '[esp-4]', '[esp+2]', '[esp+4]', '[esp+ecx*4-8]',
                         '[ebx]', '[ebx+edi*2]', '[ebp+0x40]', '[0x800]', 'fs:[esp+2]']
            sources += [mnemonic+' '+width+' ptr '+address for mnemonic in ('push','pop')
                        for width in ('word','dword') for address in addresses]
            sources += ['.byte '+','.join('0x'+raw[i:i+2] for i in range(0,len(raw),2)) for raw in
                        ('6a80','6aff','6a00','6a7f','6878563412','666a80','666aff','666a00','666a7f',
                         '6668fedc','fff4','8fc4','66fff4','668fc4','66648f442402')]
            codes, records = [], []
            for number, source in enumerate(sources):
                _, record = assemble(root, source+'\nret')
                records.append(record)
                codes.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'op%d'%number))
            unit = root/'lifted.c'
            unit.write_text('\n'.join(codes))
            subprocess.run(['cc','-shared','-fPIC','-O2','-std=c99','-Wall','-Wextra','-Werror',
                            '-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all',
                            str(unit),'-o',str(root/'lifted.so')],check=True)
            if os.environ.get('ASM_SCALAR_STACK_WCC'):
                subprocess.run([os.environ['ASM_SCALAR_STACK_WCC'],'-q','-bt=dos','-3r','-za99','-w1','-we',
                                '-fo='+str(root/'lifted.obj'),str(unit)],check=True,cwd=root)
            library = ctypes.CDLL(str(root/'lifted.so'))
            rng = random.Random(0x165057)
            size = 0x20000
            memory = bytearray(size)
            ram = (ctypes.c_uint8*size).from_buffer(memory)
            ids = [getattr(x,'UC_X86_REG_'+reg.upper()) for reg in asm_to_c.REGS]
            counts = dict(success=0, fault=0)
            for number, record in enumerate(records):
                function = getattr(library,'op%d'%number)
                function.argtypes,function.restype = [ctypes.POINTER(diff.State),ctypes.c_uint32],ctypes.c_uint
                encoded = bytes.fromhex(record['instructions'][0]['bytes'])
                scenarios = [(sp,bx) for sp in list(range(0,7))+list(range(size-6,size+7))+
                             list(range(0xfffc,0x10005))+[0xfffffffc,0xfffffffe,0xffffffff]
                             for bx in (0x800,size-1)]
                scenarios += [(0x1000+i,size+i) for i in range(-5,6)]
                scenarios += [(0x800+rng.randrange(size-0x1000),0x800+rng.randrange(size-0x1000))
                              for _ in range(64)]
                for case,(stack,base) in enumerate(scenarios):
                    original = bytearray(rng.randbytes(size))
                    original[0x100:0x100+len(encoded)] = encoded
                    memory[:] = original
                    registers = [rng.getrandbits(32) for _ in range(8)]
                    registers[1],registers[3],registers[4] = rng.randrange(8),base,stack
                    registers[5],registers[7] = 0x1400,rng.randrange(8)
                    flags = 0x202 | (rng.getrandbits(12)&0xcd5)
                    cpu = Uc(UC_ARCH_X86,UC_MODE_32)
                    cpu.mem_map(0,size)
                    cpu.mem_write(0,bytes(original))
                    selectors = diff.bind_flat_segments(cpu)
                    for reg,value in zip(ids,registers):cpu.reg_write(reg,value)
                    cpu.reg_write(x.UC_X86_REG_EFLAGS,flags)
                    failed = False
                    try:cpu.emu_start(0x100,0x100+len(encoded),count=1)
                    except UcError as error:
                        self.assertIn(error.errno,(unicorn.UC_ERR_READ_UNMAPPED,unicorn.UC_ERR_WRITE_UNMAPPED))
                        failed = True
                    state = diff.State((ctypes.c_uint32*8)(*registers),flags,0x100,0,ram,size)
                    state.segment[:] = selectors
                    state.x87_control,state.x87_status,state.x87_tag = 0x37f,0x4500,0xffff
                    state.x87_ip,state.x87_cs,state.x87_opcode = 0x123456,16,0x321
                    state.x87_dp,state.x87_ds,state.x87_compare_profile = 0x234567,16,3
                    for i in range(8):state.x87[i][:] = [i,0x80000000,0x3fff]
                    before = bytes(state)
                    context = (sources[number],case,hex(stack),hex(base))
                    self.assertEqual(function(ctypes.byref(state),1),3 if failed else 1,context)
                    self.assertEqual(state.fault,3 if failed else 0,context)
                    self.assertEqual(list(state.r),[cpu.reg_read(reg) for reg in ids],context)
                    self.assertEqual(state.pc,cpu.reg_read(x.UC_X86_REG_EIP),context)
                    self.assertEqual(state.flags,cpu.reg_read(x.UC_X86_REG_EFLAGS),context)
                    actual,expected = bytes(memory),bytes(cpu.mem_read(0,size))
                    if actual!=expected:
                        offset = next(i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
                        self.fail('%s RAM at %x: %s != %s'%(context,offset,
                                  actual[offset:offset+8].hex(),expected[offset:offset+8].hex()))
                    unchanged = diff.State.from_buffer_copy(before)
                    unchanged.r[:],unchanged.pc,unchanged.fault = state.r[:],state.pc,state.fault
                    self.assertEqual(bytes(state),bytes(unchanged),context)
                    counts['fault' if failed else 'success'] += 1
            self.assertEqual(sum(counts.values()),len(sources)*139)
            if os.environ.get('ASM_SCALAR_STACK_EVIDENCE'):
                Path(os.environ['ASM_SCALAR_STACK_EVIDENCE']).write_text(json.dumps(dict(counts=counts,
                    forms=len(sources),oracle='Unicorn '+unicorn.__version__,
                    runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                    test_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    limits='Flat Unicorn state/memory observations, not native exception-delivery proof.'),indent=2)+'\n')

    def test_encoding_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixtures = [('push ax','50','ax'),('pop eax','6658','eax'),('push ax','6651','ax'),
                        ('pop word ptr [esp]','668f0c24','WORD PTR [esp]'),
                        ('push word ptr [esp]','67ff3424','WORD PTR [esp]'),
                        ('push 1','6a02','0x1'),('push ax','f36650','ax')]
            for source,encoded,operands in fixtures:
                _,record = assemble(root,source+'\nret')
                record['instructions'][0].update(bytes=encoded,operands=operands)
                with self.assertRaises(asm_to_c.Unsupported):asm_to_c.translate(record)
