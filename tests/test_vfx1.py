# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Execute the fingerprinted game's original headset discovery and polling code."""

import importlib.util
import os
from pathlib import Path
import struct
import unittest

from tools.extract_image import extract
from tools.real_mode import MAGIC, MEMORY_SIZE, PACKET, RealMode


@unittest.skipUnless(os.environ.get('EF2000_EXE') and importlib.util.find_spec('unicorn'),
                     'requires original executable and Unicorn')
class OriginalVFX1Tests(unittest.TestCase):
    def run_original(self, address, vr, orientation=(0, 0, 0)):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
        from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                                      UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP)
        _, images = extract(Path(os.environ['EF2000_EXE']).read_bytes())
        cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        cpu.mem_map(0, 0x500000)
        cpu.mem_write(0, images['payload.bin'])
        engine = RealMode(vr=vr)
        engine.vfx_orientation = orientation
        calls = []

        def bridge(cpu, pc, size, data):
            if pc != 0x8b8:
                return
            self.assertEqual(cpu.reg_read(UC_X86_REG_EAX), 0x33)
            pointer = cpu.reg_read(UC_X86_REG_EDX)
            ax, bx, cx, dx = (struct.unpack('<I', cpu.mem_read(pointer + n, 4))[0]
                              for n in (0x1c, 0x10, 0x18, 0x14))
            calls.append((ax, bx))
            packet = PACKET.pack(MAGIC, 0, ax, bx, cx, dx, 0, 0, 0, 0, 0, 0, 0x202, 0x33)
            response, _ = engine.execute(packet, bytes(1024), bytes(MEMORY_SIZE))
            for offset, value in zip((0x1c, 0x10, 0x18, 0x14), PACKET.unpack(response)[2:6]):
                cpu.mem_write(pointer + offset, struct.pack('<I', value))
            sp = cpu.reg_read(UC_X86_REG_ESP)
            target = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EIP, target)

        cpu.hook_add(UC_HOOK_CODE, bridge)
        cpu.reg_write(UC_X86_REG_ESP, 0x4ff000)
        cpu.mem_write(0x4ff000, struct.pack('<3I', 0x4ffff0, 0x420100, 0x420104))
        cpu.reg_write(UC_X86_REG_EAX, 0x6000)
        cpu.reg_write(UC_X86_REG_EDX, 3)
        cpu.reg_write(UC_X86_REG_EBX, 0x420108)
        cpu.reg_write(UC_X86_REG_ECX, 0x42010c)
        cpu.emu_start(address, 0x4ffff0, count=10000)
        self.assertEqual(cpu.reg_read(UC_X86_REG_EIP), 0x4ffff0)
        return cpu.reg_read(UC_X86_REG_EAX), bytes(cpu.mem_read(0x420100, 16)), calls

    def test_original_discovery_selects_only_enabled_headset_slot(self):
        result, _, calls = self.run_original(0xd9b30, 'off')
        self.assertEqual(result, 0)
        self.assertEqual(len(calls), 17)
        result, _, calls = self.run_original(0xd9b30, 'vfx1')
        self.assertEqual(result, 0x6000)
        self.assertEqual(calls, [(0x607f, 0)])

    def test_original_poll_decodes_yaw_pitch_roll_and_status(self):
        result, data, calls = self.run_original(0xd9ba0, 'vfx1', (1234, -2345, 3456))
        self.assertEqual(result, 0x60)
        self.assertEqual(struct.unpack('<4I', data), (3456, 0, 1234, 0xf6d7))
        self.assertEqual(calls, [(0x6070, 0x301)])
