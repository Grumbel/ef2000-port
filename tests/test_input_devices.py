# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import struct
import unittest

from tools.input_devices import HostInput, scan_bytes, INPUT_BASE, JOYSTICK_BASE, INPUT_MAGIC


class InputTests(unittest.TestCase):
    def test_pc_make_break_extended_pause_and_modifiers(self):
        self.assertEqual(scan_bytes(4, True), b'\x1e')
        self.assertEqual(scan_bytes(4, False), b'\x9e')
        self.assertEqual(scan_bytes(82, True), b'\xe0\x48')
        self.assertEqual(scan_bytes(82, False), b'\xe0\xc8')
        self.assertEqual(scan_bytes(70, True), bytes.fromhex('e02ae037'))
        self.assertEqual(scan_bytes(72, True), bytes.fromhex('e11d45e19dc5'))
        host = HostInput()
        host.key_event(224, 0, True)
        host.key_event(4, ord('a'), True)
        self.assertEqual(host.keys, [0x1e01])
        self.assertEqual(host.shift_flags, 4)
        host.release_keys()
        self.assertFalse(host.pressed_keys)
        self.assertEqual(host.shift_flags, 0)
        self.assertEqual(bytes(host.scan_queue), bytes.fromhex('1d1e9e9d'))

    def test_pause_releases_host_state_without_sending_a_break_sequence(self):
        host = HostInput()
        host.key_event(72, 0, True)
        host.key_event(72, 0, False)
        self.assertFalse(host.pressed_keys)
        self.assertEqual(bytes(host.scan_queue), bytes.fromhex('e11d45e19dc5'))
        host.key_event(72, 0, True)
        host.release_keys()
        self.assertFalse(host.pressed_keys)

    def test_queue_backpressure_wraparound_and_release_delivery(self):
        host = HostInput()
        host.scan_queue.extend([0x1e] * 255 + [0x9e])
        memory = host.sync_input(bytes(0x100000))
        self.assertEqual(struct.unpack_from('<IHH', memory, INPUT_BASE), (INPUT_MAGIC, 0, 255))
        self.assertEqual(list(host.scan_queue), [0x9e])
        struct.pack_into('<H', memory, INPUT_BASE + 4, 1)  # Guest consumes one byte.
        memory = host.sync_input(memory)
        self.assertEqual(memory[INPUT_BASE + 8 + 255], 0x9e)
        self.assertEqual(struct.unpack_from('<HH', memory, INPUT_BASE + 4), (1, 0))
        self.assertFalse(host.scan_queue)

    def test_joystick_presence_buttons_and_full_axis_travel(self):
        host = HostInput()
        memory = host.sync_input(bytes(0x100000))
        self.assertEqual(memory[JOYSTICK_BASE:JOYSTICK_BASE + 2], b'\0\0')
        host.joystick_axes = [-32768, 0, 32767, 0]
        host.joystick_buttons = 5
        memory = host.sync_input(memory)
        self.assertEqual(struct.unpack_from('<BB4H', memory, JOYSTICK_BASE), (15, 5, 1, 64, 127, 64))
        host.joystick_axes = []
        memory = host.sync_input(memory)
        self.assertEqual(memory[JOYSTICK_BASE], 0)
