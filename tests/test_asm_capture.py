# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic ring-3 capture restoration, including expand-down stack segments."""

from dataclasses import replace
import copy
import ctypes
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
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
    @unittest.skipUnless(shutil.which('cc') and shutil.which('objdump'), 'requires C compiler and objdump')
    def test_captured_input_reaches_generated_c_without_flattening_selectors(self):
        from tools import asm_inventory, asm_to_c
        capture = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root/'fixture.bin'
            image.write_bytes(capture.guest_ram)
            instructions = asm_inventory.decode_window(image, 0x1000, 7)
            record = dict(address=0x1000, boundary='ghidra-body-ranges',
                          ghidra_instruction_layout_matches=True, overlapping_entries=[],
                          undecodable_addresses=[], ranges=[[0x1000, 0x1007]], instructions=instructions)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            (root/'lift.c').write_text(asm_to_c.translate(record))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                            '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                            str(root/'lift.c'), '-o', str(root/'lift.so')], check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'lift.so'))
            function = library.ef_lift_00001000
            function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32]
            function.restype = ctypes.c_uint
            results = asm_c_differential.isolated_probes(function, b'', 0x1000, 1, 100, False,
                                                        captured=capture)
            self.assertEqual(results[0]['state'], 'mismatch')
            self.assertIn('C status 7 (unsupported segment profile)', results[0]['differences'][0])
            # The original executed and returned using its captured stack. C's
            # unsupported selector profile remains a failure, never a pass.
            memory = bytearray(capture.guest_ram)
            memory[0x1000:0x1002] = b'\x0f\x0b'  # Original UD2 fault.
            faulting = replace(capture, guest_ram=bytes(memory))
            result = asm_c_differential.isolated_probes(function, b'', 0x1000, 1, 100, False,
                                                       captured=faulting)[0]
            self.assertEqual(result['state'], 'inconclusive')
            self.assertIn('original fault:', result['reason'])

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
