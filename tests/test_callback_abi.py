# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Execute separately compiled callback callers without game assets."""

import os
import math
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
    def test_status_sinks_use_caller_cleaned_stack_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'status.obj'
            ef2000_wcc.compile_module(
                ef2000_wcc.ROOT / 'decomp/tests/status_cdecl_contract_fixture.c',
                compiler=COMPILER, output=output)
            caller = read_module(output.read_bytes())
            entry = next(p for p in caller.publics
                         if p.symbol.rstrip('_') == 'status_cdecl_contract_fixture')
            code = bytearray(caller.segments[entry.segment].data)
            cases = [
                ('format_ui_status_pair', (0x5000, 0x6000, 11, 22)),
                ('format_ui_status_float', (0x5000, 0x6000, 0, 0x3ff40000)),
                ('format_ui_status_value', (0x5000, 0x6000, 33)),
                ('guest_fn_a4342_plain', (0x5000, 0x6000)),
                ('guest_fn_48c90', (15, 310, 5, 0x6000)),
                ('draw_tactical_mode_item', (1, 0x6000)),
            ]
            targets = {name: 0x3000 + index * 0x100
                       for index, (name, args) in enumerate(cases)}
            self.assertEqual(len(caller.fixups), len(cases))
            for fixup in caller.fixups:
                name = caller.externs[fixup.target_index].strip('_')
                struct.pack_into('<i', code, fixup.offset,
                                 targets[name] - (0x1000 + fixup.offset + 4))
            cpu = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
            cpu.mem_map(0x1000, 0x10000)
            cpu.mem_write(0x1000, bytes(code))
            for index, (name, args) in enumerate(cases):
                stub = bytearray()
                for word in range(len(args)):
                    stub.extend(b'\x8b\x44\x24' + bytes([4 + word * 4]))
                    stub.extend(b'\xa3' + struct.pack('<I', 0x9000 + index * 0x20 + word * 4))
                stub.extend(b'\xb8\x34\x12\x00\x00\xc3')
                cpu.mem_write(targets[name], bytes(stub))
            cpu.mem_write(0x9000, b'\xa5' * (len(cases) * 0x20))
            cpu.mem_write(0x10000, struct.pack('<I', 0x8000))
            cpu.reg_write(x86.UC_X86_REG_ESP, 0x10000)
            cpu.emu_start(0x1000 + entry.offset, 0x8000, count=1000)
            self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EIP), 0x8000)
            self.assertEqual(cpu.reg_read(x86.UC_X86_REG_ESP), 0x10004)
            self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EAX), 0x1234)
            for index, (name, args) in enumerate(cases):
                actual = cpu.mem_read(0x9000 + index * 0x20, 0x20)
                expected = struct.pack('<' + 'I' * len(args), *args)
                self.assertEqual(actual, expected + b'\xa5' * (0x20 - len(expected)), name)

    def test_public_trig_calls_use_x87_without_stack_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'trig.obj'
            ef2000_wcc.compile_module(
                ef2000_wcc.ROOT / 'decomp/tests/trig_contract_fixture.c',
                compiler=COMPILER, output=output)
            caller = read_module(output.read_bytes())
            entry = next(p for p in caller.publics
                         if p.symbol.rstrip('_') == 'trig_contract_fixture')
            code = bytearray(caller.segments[entry.segment].data)
            targets = {'cos_b45a2': 0x2000, 'sin_b45ac': 0x3000}
            self.assertEqual(len(caller.fixups), 2)
            for fixup in caller.fixups:
                name = caller.externs[fixup.target_index].rstrip('_')
                struct.pack_into('<i', code, fixup.offset,
                                 targets[name] - (0x1000 + fixup.offset + 4))
            for angle in (0.0, -0.75, 2.25):
                with self.subTest(angle=angle):
                    machine = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
                    machine.mem_map(0x1000, 0x10000)
                    machine.mem_write(0x1000, bytes(code))
                    # Synthetic ST0-only callees: FCOS/FSIN; RET.
                    machine.mem_write(0x2000, b'\xd9\xff\xc3')
                    machine.mem_write(0x3000, b'\xd9\xfe\xc3')
                    machine.mem_write(0x5000, struct.pack('<d', angle) + b'\xa5' * 24)
                    machine.mem_write(0x10000, struct.pack('<I', 0x4000))
                    machine.reg_write(x86.UC_X86_REG_EAX, 0x5000)
                    machine.reg_write(x86.UC_X86_REG_ESP, 0x10000)
                    machine.emu_start(0x1000 + entry.offset, 0x4000, count=1000)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x4000)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0x10004)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_EAX), 17)
                    cosine, sine = struct.unpack('<dd', machine.mem_read(0x5008, 16))
                    self.assertAlmostEqual(cosine, math.cos(angle), places=15)
                    self.assertAlmostEqual(sine, math.sin(angle), places=15)
                    self.assertEqual(machine.mem_read(0x5018, 8), b'\xa5' * 8)

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

    def test_callee_cleaned_stack_callback_result_and_live_value(self):
        self.check_callback('callback_stack1_contract_fixture',
                            'guest_callptr_stack1_ret_x',
                            b'\x8b\x44\x24\x04\x40'  # EAX = argument + 1
                            b'\xbb\xff\xff\xff\xff'
                            b'\xb9\xff\xff\xff\xff'
                            b'\xba\xff\xff\xff\xff'
                            b'\xbe\xff\xff\xff\xff'
                            b'\xbf\xff\xff\xff\xff\xc2\x04\x00', 59)

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
