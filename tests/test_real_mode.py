# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from tools.real_mode import MAGIC, MEMORY_SIZE, PACKET, RealMode
from tools.vga import indexed_rgb, save_indexed


@unittest.skipUnless(importlib.util.find_spec('unicorn'), 'requires Unicorn')
class RealModeTests(unittest.TestCase):
    def setUp(self):
        self.engine = RealMode()
        self.memory = bytes(MEMORY_SIZE)

    def call(self, vector, ax, bx=0, cx=0, dx=0, es=0, di=0, entry=0, memory=None):
        packet = PACKET.pack(MAGIC, entry, ax, bx, cx, dx, 0, di, 0, es, 0, 0, 0x202, vector)
        response, self.memory = self.engine.execute(packet, bytes(1024), self.memory if memory is None else memory)
        self.assertEqual(len(self.memory), MEMORY_SIZE)
        return PACKET.unpack(response)

    def test_host_framebuffer_snapshot_preserves_packet_and_memory(self):
        memory = bytes((i % 256 for i in range(MEMORY_SIZE)))
        result = self.call(0x100, 0x1234, cx=0x5678, dx=0x9abc, memory=memory)
        self.assertEqual(result[2], 0x1234)
        self.assertEqual(result[4:6], (0x5678, 0x9abc))
        self.assertEqual(self.memory, memory)

    def test_executes_original_16_bit_instructions_and_copies_memory_back(self):
        memory = bytearray(self.memory)
        # MOV AX,1234; MOV BX,5678; MOV DX,1000; MOV DS,DX;
        # MOV byte [0100],5a; IRET.
        code = bytes.fromhex('b8 34 12 bb 78 56 ba 00 10 8e da c6 06 00 01 5a cf')
        memory[0x10000:0x10000 + len(code)] = code
        result = self.call(0x66, 0, entry=0x10000000, memory=bytes(memory))
        self.assertEqual(result[2:4], (0x1234, 0x5678))
        self.assertEqual(result[8], 0x1000)
        self.assertEqual(self.memory[0x10100], 0x5a)
        memory = bytearray(self.memory)
        memory[0x10001:0x10003] = b'\xcd\xab'
        result = self.call(0x66, 0, entry=0x10000000, memory=bytes(memory))
        self.assertEqual(result[2], 0xabcd)

    def test_unknown_device_is_an_explicit_stop(self):
        memory = bytearray(self.memory)
        memory[0x10000:0x10005] = bytes.fromhex('ba 23 01 ec cf')
        with self.assertRaisesRegex(ValueError, 'unsupported real-mode IN port 0x123'):
            self.call(0x66, 0, entry=0x10000000, memory=bytes(memory))

    def test_failed_driver_retains_mutated_memory_and_cpu_location(self):
        memory = bytearray(self.memory)
        # PUSH CS; POP DS; MOV byte [0100],5a; MOV DX,0123; IN AL,DX; IRET.
        code = bytes.fromhex('0e1f c60600015a ba2301 ec cf')
        memory[0x10000:0x10000 + len(code)] = code
        with self.assertRaisesRegex(ValueError, 'unsupported real-mode IN port 0x123'):
            self.call(0x66, 0, entry=0x10000000, memory=bytes(memory))
        self.assertEqual(self.engine.failed_memory[0x10100], 0x5a)
        self.assertEqual(self.engine.failure_state['cs'], 0x1000)
        self.assertEqual(self.engine.failure_state['edx'], 0x123)
        # A later BIOS call must not retain a stale failed-driver snapshot.
        self.call(0x10, 0x0f00)
        self.assertIsNone(self.engine.failed_memory)
        self.assertIsNone(self.engine.failure_state)

    def test_vbe_information_modes_bank_round_trip_and_scanline(self):
        result = self.call(0x10, 0x4f00, es=0x2000, di=0x100)
        self.assertEqual(result[2], 0x4f)
        self.assertEqual(self.memory[0x20100:0x20104], b'VESA')
        self.assertEqual(struct.unpack_from('<H', self.memory, 0x20104)[0], 0x102)
        self.call(0x10, 0x4f01, cx=0x100, es=0x2000, di=0x100)
        self.assertEqual(struct.unpack_from('<3H', self.memory, 0x20110), (640, 640, 400))
        self.assertEqual(self.memory[0x20119], 8)
        self.call(0x10, 0x4f02, bx=0x100)
        result = self.call(0x10, 0x4f06, cx=640)
        self.assertEqual(result[2:5], (0x4f, 640, 640))
        memory = bytearray(self.memory)
        memory[0xa0000:0xb0000] = b'A' * 65536
        self.call(0x10, 0x4f05, dx=1, memory=bytes(memory))
        memory = bytearray(self.memory)
        memory[0xa0000:0xb0000] = b'B' * 65536
        self.call(0x10, 0x4f05, dx=0, memory=bytes(memory))
        self.assertEqual(self.memory[0xa0000:0xb0000], b'A' * 65536)
        self.call(0x10, 0x4f05, dx=1)
        self.assertEqual(self.memory[0xa0000:0xb0000], b'B' * 65536)
        self.assertEqual(self.call(0x10, 0x4f01, cx=0xdead, es=0x2000)[2], 0x14f)

    def test_mouse_buttons_and_motion_counters(self):
        self.call(0x33, 0)
        self.engine.move_mouse(5, -2)
        self.engine.mouse_buttons = 1
        result = self.call(0x33, 3)
        self.assertEqual(result[3:6], (1, 325, 98))
        result = self.call(0x33, 0xb)
        self.assertEqual(result[4:6], (5, 0xfffe))
        self.assertEqual(self.call(0x33, 0xb)[4:6], (0, 0))

    def test_motion_continues_at_all_absolute_bounds(self):
        self.call(0x33, 0)
        for dx, dy, expected in ((1000, 1000, (639, 199)),
                                 (1000, 1000, (639, 199)),
                                 (-2000, -2000, (0, 0)),
                                 (-1000, -1000, (0, 0))):
            self.engine.move_mouse(dx, dy)
            self.assertEqual(self.call(0x33, 3)[4:6], expected)
            self.assertEqual(self.call(0x33, 0xb)[4:6],
                             (dx & 0xffff, dy & 0xffff))
            self.assertEqual(self.call(0x33, 0xb)[4:6], (0, 0))

    def test_unrecognized_mission_mouse_extension_preserves_registers(self):
        for function in (0x607f, 0x617f, 0x627f, 0xffff):
            result = self.call(0x33, function, cx=0x1234, dx=0x5678)
            self.assertEqual(result[2], function)
            self.assertEqual(result[4:6], (0x1234, 0x5678))

    def test_programmatic_position_does_not_generate_motion(self):
        self.call(0x33, 4, cx=10, dx=20)
        self.assertEqual(self.call(0x33, 0xb)[4:6], (0, 0))


class FramebufferTests(unittest.TestCase):
    def test_dac_scaling_and_exported_png_pixels(self):
        colors = bytearray(768)
        colors[3:6] = bytes((63, 0, 0))
        colors[6:9] = bytes((0, 63, 0))
        self.assertEqual(indexed_rgb(bytes((1, 2)), colors), b'\xff\0\0\0\xff\0')
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'frame.png'
            save_indexed(bytes((1, 2)), colors, 2, 1, output)
            png = output.read_bytes()
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack_from('>2I', png, 16), (2, 1))
        cursor = 8
        while cursor < len(png):
            length = struct.unpack_from('>I', png, cursor)[0]
            if png[cursor + 4:cursor + 8] == b'IDAT':
                self.assertEqual(zlib.decompress(png[cursor + 8:cursor + 8 + length]),
                                 b'\0\xff\0\0\0\xff\0')
                break
            cursor += length + 12
        else:
            self.fail('missing PNG pixel data')
