# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Full generated narrowing-store effects versus independently executed x87."""

import ctypes
import json
import multiprocessing
import resource
import signal
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_softfloat, asm_c_differential as diff
from tests.test_asm_to_c import assemble
from tests.test_asm_c_program import inventory


@unittest.skipUnless(os.environ.get('ASM_SOFTFLOAT_BUILD') and platform.machine() == 'x86_64' and
                     all(shutil.which(t) for t in ('cc','as','objcopy')), 'requires x86-64 and ASM_SOFTFLOAT_BUILD')
class StoreTests(unittest.TestCase):
    def test_full_mask_rounding_pop_and_stack_effects(self):
        dependency = Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asm_softfloat.build_narrow(dependency, root/'backend')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            _, empty = assemble(root, 'ret')
            # A non-backend body first exercises late optional-header inclusion.
            codes = [asm_to_c.translate(empty).replace('ef_lift_00000100','plain_ret')]
            probes, specs = [], []
            for width, name in [(32,'dword'),(64,'qword')]:
                for pop in (False,True):
                    number = len(specs)
                    _, record = assemble(root, ('fstp' if pop else 'fst')+' '+name+' ptr [esi]\nret')
                    codes.append(asm_to_c.translate(record).replace('ef_lift_00000100','store_%d'%number))
                    opcode = ','.join(hex(v) for v in bytes.fromhex(record['instructions'][0]['bytes']))
                    probes.append('''#include <stdint.h>
uintptr_t op%d(const void *initial,void *out,void *data) {
 unsigned char saved[108]; uintptr_t pc;
 __asm__ volatile("fnsave %%[save]\\n\\tfrstor (%%[initial])\\n\\t1: .byte %s\\n\\t"
 "fnsave (%%[out])\\n\\tfrstor %%[save]\\n\\tleaq 1b(%%%%rip),%%[pc]"
 : [save] "+m"(saved),[pc] "=r"(pc) : [initial] "r"(initial),[out] "r"(out),"S"(data) : "memory"); return pc;
}'''%(number,opcode))
                    specs.append((width,pop))
            (root/'native.c').write_text('\n'.join(probes));(root/'lifted.c').write_text('\n'.join(codes))
            for name in ('native','lifted'):
                command=['cc','-shared','-fPIC','-O2','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function',
                         str(root/(name+'.c')),'-o',str(root/(name+'.so'))]
                if name=='lifted': command += [str(root/'backend/narrow.so'),'-Wl,-z,defs',
                                               '-fsanitize=undefined','-fno-sanitize-recover=all']
                subprocess.run(command,check=True)
            hardware,generated=ctypes.CDLL(str(root/'native.so')),ctypes.CDLL(str(root/'lifted.so'))
            memory=bytearray(b'\xa5'*4096);memory[0x800:0x804]=(0xf00).to_bytes(4,'little')
            ram=(ctypes.c_uint8*4096).from_buffer(memory)
            template=diff.State((ctypes.c_uint32*8)(),0xad7,0x100,0,ram,4096)
            template.r[4],template.r[6]=0x800,0x500;template.segment[:]=[16,8,24,16,16,16]
            template.x87_ip,template.x87_cs,template.x87_opcode=0x123456,16,0x321
            template.x87_dp,template.x87_ds=0x234567,16
            values=[(0x8000000000000000+i,0x3fff) for i in range(8)]
            for i,(sig,exp) in enumerate(values):template.x87[i][:]=[sig&0xffffffff,sig>>32,exp]
            baseline=bytes(template);captured=(ctypes.c_uint8*108)();data=(ctypes.c_uint8*8)()
            cases=0
            for number,(width,pop) in enumerate(specs):
                native=getattr(hardware,'op%d'%number);native.argtypes=[ctypes.c_void_p]*3;native.restype=ctypes.c_uint64
                function=getattr(generated,'store_%d'%number);function.argtypes=[ctypes.POINTER(diff.State),ctypes.c_uint32];function.restype=ctypes.c_uint
                minimum=16257 if width==32 else 15361
                fraction_bits=23 if width==32 else 52
                halfway=(1<<64)-(1<<(63-fraction_bits))
                vectors=[(0,0),(0x8000000000000000,0x3fff),
                    (0x8000000000000000+(1<<(62-fraction_bits)),0x3fff),
                    (0x8000000000000000,0x7ffe),(0x8000000000000000,1),
                    (0x8000000000000000,minimum-fraction_bits),(halfway,minimum-1),
                    (halfway+(1<<(62-fraction_bits)),minimum-1),
                    (0x8000000000000001,0x7fff),(0xc000000000000003,0x7fff),
                    (0x4000000000000000,0x3fff),(1,0),(0x8000000000000000,0),
                    (0x8000000000000000,0x7fff)]
                for masks in range(64):
                    for rounding in range(4):
                        control=0x340|masks|(rounding<<10)
                        for top in range(8):
                            for empty_top in (False,True):
                                for sig,exp0 in vectors:
                                    for sign in (0,0x8000):
                                        exp=exp0|sign
                                        tag=1 if not (exp&0x7fff) and not sig else (2 if (exp&0x7fff) in (0,0x7fff) or not sig>>63 else 0)
                                        tags=(3 if empty_top else tag)<<(top*2)
                                        initial=bytearray(108);status=0x4700|(top<<11)
                                        struct.pack_into('<H',initial,0,control);struct.pack_into('<H',initial,4,status);struct.pack_into('<H',initial,8,tags)
                                        struct.pack_into('<IHHIH',initial,12,0x123456,16,0x321,0x234567,16)
                                        current=list(values);current[top]=(sig,exp)
                                        for logical in range(8):struct.pack_into('<QH',initial,28+logical*10,*current[(top+logical)&7])
                                        data[:]=b'\xa5'*8;memory[0x500:0x508]=b'\xa5'*8
                                        pc=native((ctypes.c_uint8*108).from_buffer(initial),captured,data)
                                        state=diff.State.from_buffer_copy(baseline)
                                        state.x87_control,state.x87_status,state.x87_tag=control,status,tags
                                        state.x87[top][:]=[sig&0xffffffff,sig>>32,exp]
                                        context=(width,pop,hex(control),top,empty_top,hex(sig),hex(exp))
                                        self.assertEqual(function(ctypes.byref(state),10),0,context)
                                        observed=bytes(captured);expected_status=struct.unpack_from('<H',observed,4)[0];expected_top=(expected_status>>11)&7
                                        self.assertEqual((state.x87_status,state.x87_tag),(expected_status,struct.unpack_from('<H',observed,8)[0]),context)
                                        self.assertEqual([(r[0]|(r[1]<<32),r[2]) for r in state.x87],
                                            [struct.unpack_from('<QH',observed,28+((i-expected_top)&7)*10) for i in range(8)],context)
                                        self.assertEqual(bytes(memory[0x500:0x508]),bytes(data),context)
                                        self.assertEqual((memory[0x4ff],memory[0x508]),(0xa5,0xa5))
                                        self.assertEqual(struct.unpack_from('<I',observed,12)[0],pc&0xffffffff)
                                        self.assertEqual(struct.unpack_from('<I',observed,20)[0],ctypes.addressof(data)&0xffffffff)
                                        self.assertEqual((state.x87_ip,state.x87_cs,state.x87_dp,state.x87_ds),(0x100,8,0x500,16))
                                        self.assertEqual(state.x87_opcode,struct.unpack_from('<H',observed,18)[0]&0x7ff)
                                        self.assertEqual((state.flags,state.pc,state.r[4]),(0xad7,0xf00,0x804))
                                        cases+=1
                for address,size in [(4096-width//8+1,4096),(0,width//8-1),(0xffffffff,4096)]:
                    state=diff.State.from_buffer_copy(baseline)
                    state.r[6],state.memory_size=address,size
                    state.x87_control,state.x87_tag=0x340,3
                    before,old_ram=bytes(state),bytes(memory)
                    self.assertEqual(function(ctypes.byref(state),10),3)
                    state.fault=0
                    self.assertEqual(bytes(state),before)
                    self.assertEqual(bytes(memory),old_ram)
                    state.x87_status=0x8081
                    before=bytes(state)
                    self.assertEqual(function(ctypes.byref(state),10),8)
                    self.assertEqual(bytes(state),before)
                state=diff.State.from_buffer_copy(baseline)
                state.r[6],state.x87_control=4096-width//8,0x37f
                self.assertEqual(function(ctypes.byref(state),10),0)
                expected=(0x3f800000 if width==32 else 0x3ff0000000000000).to_bytes(width//8,'little')
                self.assertEqual(bytes(memory[4096-width//8:]),expected)
                for pending in (False,True):
                    for mask,sig,exp,tag in [(0,0x8000000000000000,0x3fff,3),
                        (0x3e,0x8000000000000001,0x7fff,2),(0x37,0x8000000000000000,0x7ffe,0),
                        (0x2f,0x8000000000000000,1,0),(0x1f,0x8000008000000000,0x3fff,0)]:
                        def fault():
                            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
                            initial=bytearray(108)
                            struct.pack_into('<H',initial,0,0x340|(0x3e if pending else mask))
                            struct.pack_into('<H',initial,4,0x8081 if pending else 0)
                            struct.pack_into('<H',initial,8,0xfffc|tag)
                            struct.pack_into('<QH',initial,28,sig,exp)
                            native((ctypes.c_uint8*108).from_buffer(initial),captured,None)
                        process=multiprocessing.get_context('fork').Process(target=fault)
                        process.start();process.join()
                        self.assertEqual(process.exitcode,-signal.SIGFPE if pending else -signal.SIGSEGV)
            self.assertEqual(cases,458752)

    def test_backend_is_mandatory_and_bound_before_compilation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);image,inv,_=inventory(root,'.type root,@function\nroot: fld1\nfstp dword ptr [esi]\nret\n.size root,.-root')
            out=root/'out';report=asm_to_c.generate(inv,out,image)
            self.assertTrue(report['required_backends'])
            with self.assertRaisesRegex(ValueError,'require --softfloat-build'):
                diff.verify(out,root/'evidence',image)
            self.assertFalse((root/'evidence').exists())
            for field,value,message in [('required_backends',[],'backend declaration'),
                                        ('backend_source_sha256','0'*64,'adapter source digest')]:
                altered=dict(report);altered[field]=value
                (out/'report.json').write_text(json.dumps(altered))
                with self.assertRaisesRegex(ValueError,message):
                    diff.verify(out,root/'evidence',image,compiler='must-not-run',
                                softfloat_build=Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve())
                self.assertFalse((root/'evidence').exists())
