# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Opt-in SDL integration tests: EF2000_TEST_DISPLAY must name a private Xvfb."""

import ctypes as c
import os
import struct
import unittest

from tools.display import Display
from tools.input_devices import HostInput


@unittest.skipUnless(os.environ.get('EF2000_TEST_DISPLAY'), 'requires explicit private Xvfb display')
class SDLInputTests(unittest.TestCase):
    def setUp(self):
        self.environment = os.environ.get('DISPLAY')
        os.environ['DISPLAY'] = os.environ['EF2000_TEST_DISPLAY']
        self.display = Display()
        self.host = HostInput()
        self.host.mouse_limits = [0, 639, 0, 399]
        self.host.move_mouse = lambda *args: None
        self.display.poll(self.host)
        self.display.focused = True

    def tearDown(self):
        self.display.close()
        if self.environment is None:
            os.environ.pop('DISPLAY', None)
        else:
            os.environ['DISPLAY'] = self.environment

    def event(self, kind, scan=0, key=0):
        data = bytearray(64)
        struct.pack_into('<I', data, 0, kind)
        struct.pack_into('<ii', data, 16, scan, key)
        self.display.sdl.SDL_PushEvent.argtypes = [c.c_void_p]
        self.assertEqual(self.display.sdl.SDL_PushEvent(c.create_string_buffer(bytes(data))), 1)

    def test_sdl_keyboard_events_and_focus_loss_release_held_keys(self):
        self.event(0x300, 224, 0)
        self.event(0x300, 82, 0)
        self.event(0x301, 82, 0)
        self.display.poll(self.host)
        self.assertEqual(bytes(self.host.scan_queue), bytes.fromhex('1de048e0c8'))
        data = bytearray(64)
        struct.pack_into('<I', data, 0, 0x200)
        data[12] = 13
        self.display.sdl.SDL_PushEvent(c.create_string_buffer(bytes(data)))
        self.display.poll(self.host)
        self.assertEqual(bytes(self.host.scan_queue), bytes.fromhex('1de048e0c89d'))
        self.assertFalse(self.host.pressed_keys)
        self.assertEqual(self.host.shift_flags, 0)

    def test_virtual_joystick_axes_buttons_focus_and_hotplug(self):
        sdl = self.display.sdl
        for name, args, result in (
            ('SDL_JoystickAttachVirtual', [c.c_int, c.c_int, c.c_int, c.c_int], c.c_int),
            ('SDL_JoystickDetachVirtual', [c.c_int], c.c_int),
            ('SDL_JoystickSetVirtualAxis', [c.c_void_p, c.c_int, c.c_int16], c.c_int),
            ('SDL_JoystickSetVirtualButton', [c.c_void_p, c.c_int, c.c_uint8], c.c_int),
        ):
            getattr(sdl, name).argtypes = args
            getattr(sdl, name).restype = result
        device = sdl.SDL_JoystickAttachVirtual(4, 4, 4, 0)
        self.assertGreaterEqual(device, 0, sdl.SDL_GetError())
        try:
            self.display.poll_joystick(self.host)
            stick = self.display.joystick
            self.assertTrue(stick)
            values = [-32768, 0, 32767, -12345]
            for axis, value in enumerate(values):
                self.assertEqual(sdl.SDL_JoystickSetVirtualAxis(stick, axis, value), 0)
            self.assertEqual(sdl.SDL_JoystickSetVirtualButton(stick, 0, 1), 0)
            self.assertEqual(sdl.SDL_JoystickSetVirtualButton(stick, 3, 1), 0)
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, values)
            self.assertEqual(self.host.joystick_buttons, 9)
            self.display.joystick_axes = [(2, -1), (0, 1)]
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [-32767, -32768])
            self.display.joystick_axes = [(i, 1) for i in range(4)]
            self.display.focused = False
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [0, 0, 0, 0])
            self.assertEqual(self.host.joystick_buttons, 0)
        finally:
            self.assertEqual(sdl.SDL_JoystickDetachVirtual(device), 0)
        self.display.poll_joystick(self.host)
        self.assertEqual(self.host.joystick_axes, [])
        self.assertEqual(self.host.joystick_buttons, 0)
