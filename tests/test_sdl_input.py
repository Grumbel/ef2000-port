# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Opt-in SDL integration tests: EF2000_TEST_DISPLAY must name a private Xvfb."""

import ctypes as c
import os
import struct
import tempfile
import sys
from pathlib import Path
from unittest.mock import patch
import unittest

from tools.display import Display
from tools.input_devices import HostInput


def virtual_api(sdl):
    for name, args, result in (
        ('SDL_JoystickAttachVirtual', [c.c_int, c.c_int, c.c_int, c.c_int], c.c_int),
        ('SDL_JoystickDetachVirtual', [c.c_int], c.c_int),
        ('SDL_JoystickSetVirtualAxis', [c.c_void_p, c.c_int, c.c_int16], c.c_int),
        ('SDL_JoystickSetVirtualButton', [c.c_void_p, c.c_int, c.c_uint8], c.c_int),
    ):
        getattr(sdl, name).argtypes = args
        getattr(sdl, name).restype = result


@unittest.skipUnless(os.environ.get('EF2000_TEST_DISPLAY'), 'requires explicit private Xvfb display')
class SDLInputTests(unittest.TestCase):
    def setUp(self):
        self.environment = os.environ.get('DISPLAY')
        os.environ['DISPLAY'] = os.environ['EF2000_TEST_DISPLAY']
        self.display = Display(joystick='off')
        self.host = HostInput()
        self.host.mouse_limits = [0, 639, 0, 399]
        self.host.move_mouse = lambda *args: None
        self.display.poll(self.host)
        self.display.focused = True

    def tearDown(self):
        if self.display:
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
        virtual_api(sdl)
        device = sdl.SDL_JoystickAttachVirtual(4, 4, 4, 0)
        self.assertGreaterEqual(device, 0, sdl.SDL_GetError())
        try:
            self.display.joystick_selection = str(device)
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


    def test_second_joystick_selection_and_missing_device(self):
        sdl = self.display.sdl
        virtual_api(sdl)
        devices = [sdl.SDL_JoystickAttachVirtual(4, 2, 2, 0) for _ in range(2)]
        self.assertTrue(all(device >= 0 for device in devices))
        try:
            self.display.joystick_selection = str(devices[1])
            self.display.poll_joystick(self.host)
            stick = self.display.joystick
            self.assertTrue(stick)
            sdl.SDL_JoystickSetVirtualAxis(stick, 0, 18000)
            sdl.SDL_JoystickSetVirtualButton(stick, 1, 1)
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [18000, 0])
            self.assertEqual(self.host.joystick_buttons, 2)
            self.assertEqual(sdl.SDL_JoystickDetachVirtual(devices.pop()), 0)
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [])
            self.assertEqual(self.host.joystick_buttons, 0)
        finally:
            for device in reversed(devices):
                sdl.SDL_JoystickDetachVirtual(device)

    def test_supervisor_selects_automatic_controls_before_launch(self):
        from tools.real_mode import run
        self.display.close()
        self.display = None

        class WithVirtualJoystick(Display):
            def __init__(self, **kwargs):
                super().__init__(**kwargs)
                virtual_api(self.sdl)
                self.device = self.sdl.SDL_JoystickAttachVirtual(4, 4, 4, 0)
                if kwargs.get('joystick') != 'off':
                    self.joystick_selection = str(self.device)

            def close(self):
                if hasattr(self, 'device'):
                    self.sdl.SDL_JoystickDetachVirtual(self.device)
                    del self.device
                super().close()

        for joystick, expected in (('auto', b'1'), ('off', b'0')):
            with self.subTest(joystick=joystick), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                config = output / 'drive/EF2000/EF2000.CFG'
                config.parent.mkdir(parents=True)
                config.write_bytes(b'CONTROL=9\r\nDETAIL=2\r\n')
                with patch('tools.display.Display', WithVirtualJoystick), (output / 'calls.log').open('w') as log:
                    process, _ = run([sys.executable, '-c', 'pass'], output, log,
                                     headless=False, controls='auto', joystick=joystick)
                self.assertEqual(process.returncode, 0)
                self.assertEqual(config.read_bytes(), b'CONTROL=' + expected + b'\r\nDETAIL=2\r\n')
