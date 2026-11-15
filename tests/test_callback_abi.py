# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Execute separately compiled callback callers without game assets."""

import os
from pathlib import Path
import shutil
import struct
import tempfile
import unittest

from tools import ef2000_wcc
from tools.omf_splice import read_module

try:
    import unicorn as uc
    from unicorn import x86_const as x86
except ImportError:
    uc = None

COMPILER = os.environ.get('EF2000_WCC386') or shutil.which('wcc386')


@unittest.skipUnless(COMPILER and uc, 'requires OpenWatcom and Unicorn')
class CallbackAbiTests(unittest.TestCase):
    def test_caller_preserves_live_ecx_across_callback_helper(self):
        self.check_callback('callback_contract_fixture', 'guest_callptr_eax_ret_x',
                            b'\x40\xc3', 41)

    def test_caller_preserves_live_value_across_draw_callback_helper(self):
        self.check_callback('callback_zero_contract_fixture', 'guest_callptr0_x',
                            b'\xbb\xff\xff\xff\xff'  # EBX
                            b'\xb9\xff\xff\xff\xff'  # ECX
                            b'\xba\xff\xff\xff\xff'  # EDX
                            b'\xbe\xff\xff\xff\xff'  # ESI
                            b'\xbf\xff\xff\xff\xff\xc3', 17)  # EDI; ret

    def check_callback(self, fixture, helper_name, callback, expected):
        with tempfile.TemporaryDirectory() as directory:
            objects = []
            for source in (f'decomp/tests/{fixture}.c',
                           'decomp/src/guest_callptr1.c'):
                output = Path(directory) / (Path(source).stem + '.obj')
                ef2000_wcc.compile_module(ef2000_wcc.ROOT / source,
                                         compiler=COMPILER, output=output)
                objects.append(read_module(output.read_bytes()))
            caller, helpers = objects
            public = next(p for p in helpers.publics
                          if p.symbol.rstrip('_') == helper_name)
            end = min((p.offset for p in helpers.publics
                       if p.segment == public.segment and p.offset > public.offset),
                      default=helpers.segments[public.segment].size)
            self.assertFalse(any(f.segment == public.segment and
                                 public.offset <= f.offset < end
                                 for f in helpers.fixups))
            helper = helpers.segments[public.segment].data[public.offset:end]
            entry = next(p for p in caller.publics
                         if p.symbol.rstrip('_') == fixture)
            code = bytearray(caller.segments[entry.segment].data)
            self.assertEqual(len(caller.fixups), 1)
            fixup = caller.fixups[0]
            self.assertEqual(caller.externs[fixup.target_index].rstrip('_'),
                             helper_name)
            struct.pack_into('<i', code, fixup.offset,
                             0x2000 - (0x1000 + fixup.offset + 4))
            machine = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
            machine.mem_map(0x1000, 0x10000)
            machine.mem_write(0x1000, bytes(code))
            machine.mem_write(0x2000, bytes(helper))
            machine.mem_write(0x3000, callback)
            machine.mem_write(0x10000, struct.pack('<I', 0x4000))
            for reg, value in ((x86.UC_X86_REG_EAX, 0x3000),
                               (x86.UC_X86_REG_EDX, 17),
                               (x86.UC_X86_REG_ECX, 23),
                               (x86.UC_X86_REG_ESP, 0x10000)):
                machine.reg_write(reg, value)
            machine.emu_start(0x1000 + entry.offset, 0x4000, count=100)
            self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x4000)
            self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0x10004)
            self.assertEqual(machine.reg_read(x86.UC_X86_REG_EAX), expected)
