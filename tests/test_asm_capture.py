# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic ring-3 capture restoration, including expand-down stack segments."""

from dataclasses import replace
import copy
import struct
import unittest

from tools import asm_capture, asm_c_differential, call_capture


def fixture():
    ram = bytearray(0x20000)
    # Reload SS after restoration: this requires actual CPL=3, not just CS RPL=3.
    ram[0x1000:0x1007] = bytes.fromhex('66b807008ed0c3')
    ram[0x18000:0x18004] = struct.pack('<I', 0x1100)
    code = 0x00cffb000000ffff
    data = 0x0040f70000000100  # Expand-down, 32-bit, lower limit 0x100.
    ldt = struct.pack('<QQ', data, code)
    ldt_descriptor = 15 | (0x30100 << 16) | (0x82 << 40)
    gdt = struct.pack('<QQ', 0, ldt_descriptor)
    tables = {'gdtr': dict(selector=0, base=0x30000, limit=15, flags=0, bytes=gdt),
              'ldtr': dict(selector=8, base=0x30100, limit=15, flags=0, bytes=ldt),
              'descriptors': dict(cs=code, ss=data, ds=data, es=data, fs=0, gs=0)}
    regs = dict(eax=11, ebx=22, ecx=33, edx=44, esi=55, edi=66, ebp=77,
                esp=0x18000, eip=0x1000, eflags=0x246, cs=15, ss=7, ds=7, es=7, fs=0, gs=0)
    x87 = dict(x87_control=0x37f, x87_status=5 << 11, x87_tag=0xffff,
               x87_ip=0x1234, x87_cs=15, x87_dp=0x2345, x87_ds=7, x87_opcode=0x321,
               x87=[[i, 0x80000000, 0x3fff+i] for i in range(8)])
    return call_capture.CapturedCall(bytes(32), 0x1000, 0, regs, bytes(ram),
                                     2, x87, tables, 0x02010400)


class AsmCaptureTests(unittest.TestCase):
    def test_restore_privilege_memory_tables_and_physical_x87(self):
        from unicorn import UcError, x86_const
        capture = fixture()
        cpu = asm_capture.restore(capture)
        self.assertEqual(bytes(cpu.mem_read(0, len(capture.guest_ram))), capture.guest_ram)
        self.assertEqual(asm_c_differential.capture_x87(cpu), capture.x87)
        for name in ('gdtr', 'ldtr'):
            table = capture.table_context[name]
            self.assertEqual(bytes(cpu.mem_read(table['base'], len(table['bytes']))), table['bytes'])
            self.assertEqual(cpu.reg_read(getattr(x86_const, 'UC_X86_REG_'+name.upper()))[:3],
                             tuple(table[k] for k in ('selector', 'base', 'limit')))
        with self.assertRaises(UcError):
            cpu.mem_read(0x10000000, 1)
        cpu.emu_start(0x1000, 0x1100, count=3)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_EIP), 0x1100)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_SS), 7)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_ESP), 0x18004)

    def test_unsupported_context_is_rejected(self):
        capture = fixture()
        with self.assertRaisesRegex(ValueError, 'EFCAP002'):
            asm_capture.restore(replace(capture, format_version=1))
        for change, message in (
                (lambda c: c.table_context['gdtr'].update(bytes=None), 'complete'),
                (lambda c: c.table_context['ldtr'].update(base=0x10000000), 'overlaps'),
                (lambda c: c.table_context['descriptors'].update(ds=None), 'unavailable'),
                (lambda c: c.table_context['descriptors'].update(ds=0x0040f70000010100), 'zero-base'),
                (lambda c: c.registers.update(cs=12), 'ring-3'),
                (lambda c: c.registers.update(eflags=0x202 | (1 << 17)), 'VM86'),
                (lambda c: c.registers.update(eflags=0x202 | (1 << 14)), 'nested-task')):
            current = copy.deepcopy(capture)
            change(current)
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                asm_capture.restore(current)


if __name__ == '__main__':
    unittest.main()
