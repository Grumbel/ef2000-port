# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Execute compiler runtime helper contracts without original game data."""

import math
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
class WatcomRuntimeHelperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        output = Path(cls.directory.name) / 'runtime.obj'
        ef2000_wcc.compile_module(
            ef2000_wcc.ROOT / 'decomp/src/watcom_rt_helpers.c',
            compiler=COMPILER, output=output)
        cls.module = read_module(output.read_bytes())

    def machine(self, name, *, stack_args=b'', registers=None):
        entry = next(p for p in self.module.publics if p.symbol == name)
        # These helpers are self-contained assembly, or call through guest RAM.
        end = min((p.offset for p in self.module.publics
                   if p.segment == entry.segment and p.offset > entry.offset),
                  default=self.module.segments[entry.segment].size)
        self.assertFalse(any(f.segment == entry.segment and
                             entry.offset <= f.offset < end
                             for f in self.module.fixups))
        machine = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
        machine.mem_map(0x1000, 0x10000)
        machine.mem_map(0x132000, 0x1000)
        machine.mem_write(0x1000, bytes(self.module.segments[entry.segment].data))
        machine.mem_write(0xf000, struct.pack('<I', 0x8000) + stack_args)
        seeds = dict(eax=0x10203040, ebx=0x21324354, ecx=0x32435465,
                     edx=0x43546576, esi=0x54657687, edi=0x65768798,
                     ebp=0x768798a9)
        seeds.update(registers or {})
        for reg, value in seeds.items():
            machine.reg_write(getattr(x86, 'UC_X86_REG_' + reg.upper()), value)
        machine.reg_write(x86.UC_X86_REG_ESP, 0xf000)
        machine.reg_write(x86.UC_X86_REG_FPCW, 0x37f)
        machine.reg_write(x86.UC_X86_REG_FPTAG, 0xffff)
        return machine, 0x1000 + entry.offset, seeds

    def test_math_helpers_return_with_registers_and_stack_intact(self):
        cases = [
            ('IF@DSQRT', (9.0,), 3.0),
            ('IF@DSIN', (0.75,), math.sin(0.75)),
            ('IF@DCOS', (0.75,), math.cos(0.75)),
            ('IF@DLOG', (2.0,), math.log(2.0)),
            ('IF@DTAN', (0.75,), math.tan(0.75)),
            ('IF@DATAN', (-0.75,), math.atan(-0.75)),
            ('IF@DFMOD', (7.5, 2.0), 1.5),
            ('IF@DATAN2', (3.0, 4.0), math.atan2(3.0, 4.0)),
        ]
        for name, inputs, expected in cases:
            with self.subTest(helper=name):
                machine, entry, seeds = self.machine(name)
                setup = bytearray()
                for index, value in enumerate(inputs):
                    address = 0x9008 + 8 * index
                    machine.mem_write(address, struct.pack('<d', value))
                    setup.extend(b'\xdd\x05' + struct.pack('<I', address))
                setup.extend(b'\xe9' + struct.pack('<i', entry - (0x7000 + len(setup) + 5)))
                machine.mem_write(0x7000, bytes(setup))
                # Store the returned ST0 after the helper's own RET executes.
                machine.mem_write(0x8000, b'\xdd\x1d\x00\x90\x00\x00')
                machine.emu_start(0x7000, 0x8006, count=1000)
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x8006)
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0xf004)
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_FPCW), 0x37f)
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_FPTAG), 0xffff)
                for reg, value in seeds.items():
                    self.assertEqual(machine.reg_read(getattr(x86, 'UC_X86_REG_' + reg.upper())), value, reg)
                self.assertAlmostEqual(struct.unpack('<d', machine.mem_read(0x9000, 8))[0], expected, places=15)

    def test_rounding_helpers_pop_the_stack_double_and_restore_control_word(self):
        for name, operation in [('ceil', math.ceil), ('floor', math.floor), ('trunc', math.trunc)]:
            for value in (-1.75, 0.0, 2.25):
                with self.subTest(helper=name, value=value):
                    machine, entry, seeds = self.machine(name, stack_args=struct.pack('<d', value))
                    machine.mem_write(0x8000, b'\xdd\x1d\x00\x90\x00\x00')
                    machine.emu_start(entry, 0x8006, count=1000)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x8006)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0xf00c)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_FPCW), 0x37f)
                    self.assertEqual(machine.reg_read(x86.UC_X86_REG_FPTAG), 0xffff)
                    for reg, initial in seeds.items():
                        self.assertEqual(machine.reg_read(getattr(x86, 'UC_X86_REG_' + reg.upper())), initial, reg)
                    self.assertEqual(struct.unpack('<d', machine.mem_read(0x9000, 8))[0], operation(value))

    def test_diagnostic_thunks_forward_cdecl_arguments_and_return(self):
        cases = [
            ('guest_fatal_arg_', 2), ('guest_fatal_arg3_', 4),
            ('guest_report_parser_enum_error_', 3),
            ('guest_parser_line_overflow_', 1),
            ('report_buffer_capacity_exceeded_', 1),
        ]
        for name, count in cases:
            with self.subTest(helper=name):
                machine, entry, seeds = self.machine(name)
                machine.mem_write(0x132a74, struct.pack('<I', 0x6000))
                machine.mem_write(0x6000, b'\xc3')
                observed = []
                def capture(cpu, address, size, data):
                    sp = cpu.reg_read(x86.UC_X86_REG_ESP)
                    observed.append(struct.unpack('<' + 'I' * count, cpu.mem_read(sp + 4, count * 4)))
                machine.hook_add(uc.UC_HOOK_CODE, capture, begin=0x6000, end=0x6000)
                machine.emu_start(entry, 0x8000, count=1000)
                self.assertEqual(observed, [tuple(seeds[reg] for reg in ('eax', 'edx', 'ebx', 'ecx')[:count])])
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x8000)
                self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0xf004)

    def test_callback_thunk_forwards_register_and_stack_arguments(self):
        machine, entry, seeds = self.machine(
            'guest_callptr4s2_x_', stack_args=struct.pack('<III', 44, 55, 66),
            registers=dict(eax=0x6000, edx=11, ebx=22, ecx=33))
        machine.mem_write(0x6000, b'\xc2\x08\x00')
        observed = []
        def capture(cpu, address, size, data):
            regs = tuple(cpu.reg_read(getattr(x86, 'UC_X86_REG_' + reg)) for reg in ('EAX', 'EDX', 'EBX', 'ECX'))
            sp = cpu.reg_read(x86.UC_X86_REG_ESP)
            observed.append((regs, struct.unpack('<II', cpu.mem_read(sp + 4, 8))))
        machine.hook_add(uc.UC_HOOK_CODE, capture, begin=0x6000, end=0x6000)
        machine.emu_start(entry, 0x8000, count=1000)
        self.assertEqual(observed, [((11, 22, 33, 44), (55, 66))])
        self.assertEqual(machine.reg_read(x86.UC_X86_REG_EIP), 0x8000)
        self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESP), 0xf010)
        self.assertEqual(machine.reg_read(x86.UC_X86_REG_ESI), seeds['esi'])
