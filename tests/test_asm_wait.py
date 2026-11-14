# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""WAIT grouping, independent boundaries, pending exceptions and instruction fuel."""

import ctypes
import multiprocessing
from pathlib import Path
import platform
import resource
import shutil
import signal
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff, asm_x87_oracle
from tests.test_asm_to_c import assemble, unicorn
from tests.test_asm_c_x87 import seeded_x87


@unittest.skipUnless(unicorn and platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Linux x86-64, Unicorn, binutils and cc')
class WaitTests(unittest.TestCase):
    def compiled(self, root, source):
        image, record = assemble(root, source)
        (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
        (root/'lift.c').write_text(asm_to_c.translate(record))
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', str(root/'lift.c'), '-o', str(root/'lift.so')], check=True)
        library = ctypes.CDLL(str(root/'lift.so'))
        function = library.ef_lift_00000100
        function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
        return image, record, function

    def test_waiting_aliases_have_separate_wait_and_no_wait_operations(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native, _ = asm_x87_oracle.build(root/'native')
            image, record, function = self.compiled(root,
                'fstcw word ptr [edi]\nfwait\nfldcw word ptr [edi]\nfstsw ax\nret')
            self.assertEqual([i['mnemonic'] for i in record['instructions']],
                             ['fwait', 'fnstcw', 'fwait', 'fldcw', 'fwait', 'fnstsw', 'ret'])
            result = diff.compare(function, image.read_bytes(), 0x100,
                                  [17, 19, 23, 29, 0x3f0000, 31, 37, 0x300400], 0xad7,
                                  initial_x87=seeded_x87(), native_x87=native)
            self.assertEqual(result['state'], 'pass', result)
            residual = root/'residual'
            residual.mkdir()
            image, record, function = self.compiled(residual, 'fstcw word ptr [edi]\nret')
            self.assertEqual(record['instructions'][1]['address'], 0x101)
            # Entering the residual nonwaiting instruction must bypass WAIT's
            # pending check without losing any x87 state or store effects.
            result = diff.compare(function, image.read_bytes(), 0x101,
                                  [17, 19, 23, 29, 0x3f0000, 31, 37, 0x300400], 0xad7,
                                  initial_x87=dict(x87_control=0x37e, x87_status=0x8081), native_x87=native)
            self.assertEqual(result['state'], 'pass', result)

    def test_wait_consumes_fuel_and_stops_before_any_store_on_pending(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, function = self.compiled(root, 'fstcw word ptr [edi]\nret')
            ram = (ctypes.c_uint8*4096)(*([0xa5]*4096))
            state = diff.State((ctypes.c_uint32*8)(), 0xad7, 0x100, 0, ram, 4096)
            state.r[4], state.r[7] = 0x800, 0x500
            state.segment[:] = [16, 8, 16, 16, 16, 16]
            diff.set_x87(state, dict(seeded_x87(), x87_status=0x6d21))
            before, previous_ram = bytes(state), bytes(ram)
            self.assertEqual(function(ctypes.byref(state), 1), 1)
            self.assertEqual(state.pc, 0x101)
            state.pc = 0x100
            self.assertEqual(bytes(state), before)
            self.assertEqual(bytes(ram), previous_ram)
            state.x87_control, state.x87_status = 0x37e, 0x8081
            before = bytes(state)
            self.assertEqual(function(ctypes.byref(state), 20), 8)
            self.assertEqual(bytes(state), before)
            self.assertEqual(bytes(ram), previous_ram)
            # The residual FNSTCW is a real independent entry and is nonwaiting.
            state.pc = 0x101
            self.assertEqual(function(ctypes.byref(state), 1), 1)
            self.assertEqual(bytes(ram[0x500:0x502]), b'\x7e\x03')
            self.assertEqual(state.pc, 0x103)
            self.assertEqual(state.x87_status, 0x8081)

    def test_actual_wait_traps_before_the_following_native_store(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root/'hardware.c'
            source.write_text(r'''#include <stdint.h>
void wait_store(const void *initial,void *out) {
 unsigned char saved[108];
 __asm__ volatile("fnsave %[save]\n\tfrstor (%[initial])\n\tfwait\n\tfnstcw (%[out])\n\tfrstor %[save]"
 : [save] "+m"(saved) : [initial] "r"(initial),[out] "r"(out) : "memory");
}
''')
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', str(source), '-o', str(root/'hardware.so')], check=True)
            function = ctypes.CDLL(str(root/'hardware.so')).wait_store
            function.argtypes, function.restype = [ctypes.c_void_p]*2, None
            output = multiprocessing.RawArray(ctypes.c_uint8, [0xa5, 0xa5])
            for pending in (False, True):
                output[:] = [0xa5, 0xa5]
                def work():
                    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                    values = dict(x87_control=0x37e, x87_status=0x8081 if pending else 0,
                                  x87_tag=0xffff, x87_opcode=0, x87=[[0, 0, 0] for _ in range(8)])
                    initial = (ctypes.c_uint8*108).from_buffer(asm_x87_oracle.pack(values))
                    function(initial, output)
                child = multiprocessing.get_context('fork').Process(target=work)
                child.start(); child.join()
                self.assertEqual(child.exitcode, -signal.SIGFPE if pending else 0)
                self.assertEqual(bytes(output), b'\xa5\xa5' if pending else b'\x7e\x03')
