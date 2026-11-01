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

from tools.display import (Display, Event, KEY_DOWN, KEY_UP, MOUSE_BUTTON_DOWN,
                           WINDOW_FOCUS_GAINED, WINDOW_FOCUS_LOST)
from tools.input_devices import HostInput


class VirtualJoystickDesc(c.Structure):
    _fields_ = [('version', c.c_uint32), ('type', c.c_uint16), ('padding', c.c_uint16),
                ('vendor', c.c_uint16), ('product', c.c_uint16),
                ('axes', c.c_uint16), ('buttons', c.c_uint16), ('balls', c.c_uint16),
                ('hats', c.c_uint16), ('touchpads', c.c_uint16), ('sensors', c.c_uint16),
                ('padding2', c.c_uint16 * 2), ('button_mask', c.c_uint32),
                ('axis_mask', c.c_uint32), ('name', c.c_char_p),
                ('touchpads_ptr', c.c_void_p), ('sensors_ptr', c.c_void_p),
                ('userdata', c.c_void_p), ('callbacks', c.c_void_p * 8)]


def virtual_api(sdl):
    for name, args, result in (
        ('SDL_AttachVirtualJoystick', [c.POINTER(VirtualJoystickDesc)], c.c_uint32),
        ('SDL_DetachVirtualJoystick', [c.c_uint32], c.c_bool),
        ('SDL_SetJoystickVirtualAxis', [c.c_void_p, c.c_int, c.c_int16], c.c_bool),
        ('SDL_SetJoystickVirtualButton', [c.c_void_p, c.c_int, c.c_bool], c.c_bool),
    ):
        getattr(sdl, name).argtypes = args
        getattr(sdl, name).restype = result


def attach_virtual(sdl, axes=4, buttons=4, hats=0):
    desc = VirtualJoystickDesc(version=c.sizeof(VirtualJoystickDesc), type=0,
                               axes=axes, buttons=buttons, hats=hats,
                               name=b'EF2000 test joystick')
    return sdl.SDL_AttachVirtualJoystick(c.byref(desc))


def device_index(sdl, identity):
    count = c.c_int()
    devices = sdl.SDL_GetJoysticks(c.byref(count))
    try:
        return next(index for index in range(count.value) if devices[index] == identity)
    finally:
        sdl.SDL_free(devices)


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
        event = Event()
        event.key.type, event.key.scan, event.key.key = kind, scan, key
        self.push_event(event)

    def push_event(self, event):
        self.display.sdl.SDL_PushEvent.argtypes = [c.POINTER(Event)]
        self.display.sdl.SDL_PushEvent.restype = c.c_bool
        self.assertTrue(self.display.sdl.SDL_PushEvent(c.byref(event)))

    def focus_event(self, focused):
        event = Event()
        event.window.type = WINDOW_FOCUS_GAINED if focused else WINDOW_FOCUS_LOST
        self.push_event(event)

    def click_event(self, button=1, x=200, y=200):
        event = Event()
        event.button.type, event.button.button = MOUSE_BUTTON_DOWN, button
        event.button.x, event.button.y = x, y
        self.push_event(event)

    def relative_mode(self):
        self.display.sdl.SDL_GetWindowRelativeMouseMode.argtypes = [c.c_void_p]
        self.display.sdl.SDL_GetWindowRelativeMouseMode.restype = c.c_bool
        return bool(self.display.sdl.SDL_GetWindowRelativeMouseMode(self.display.window))

    def test_capture_requires_click_and_never_returns_on_focus(self):
        self.focus_event(True)
        self.display.poll(self.host)
        self.assertFalse(self.display.captured)
        self.assertFalse(self.relative_mode())
        self.click_event(button=3)
        self.click_event(x=-1)
        self.display.poll(self.host)
        self.assertFalse(self.display.captured)
        self.click_event()
        self.display.poll(self.host)
        self.assertTrue(self.display.captured)
        self.assertTrue(self.relative_mode())
        self.focus_event(False)
        self.display.poll(self.host)
        self.assertFalse(self.display.captured)
        self.assertFalse(self.relative_mode())
        self.focus_event(True)
        self.display.poll(self.host)
        self.assertFalse(self.display.captured)
        self.assertFalse(self.relative_mode())

    def test_escape_hold_releases_mouse_without_autorepeat_resetting_deadline(self):
        self.click_event()
        self.display.poll(self.host)
        with patch('tools.display.time.monotonic', return_value=100):
            self.event(KEY_DOWN, 41, 27)
            self.display.poll(self.host)
        self.assertTrue(self.display.captured)
        with patch('tools.display.time.monotonic', return_value=102.9):
            event = Event()
            event.key.type, event.key.scan, event.key.key, event.key.repeat = KEY_DOWN, 41, 27, True
            self.push_event(event)
            self.display.poll(self.host)
        self.assertTrue(self.display.captured)
        with patch('tools.display.time.monotonic', return_value=103):
            self.display.poll(self.host)
        self.assertFalse(self.display.captured)
        self.assertFalse(self.relative_mode())
        self.assertIn(41, self.host.pressed_keys)  # Escape still reaches the game.
        self.event(KEY_UP, 41, 27)
        self.display.poll(self.host)
        self.assertNotIn(41, self.host.pressed_keys)

    def test_short_escape_and_focus_loss_cancel_the_release_timer(self):
        self.click_event()
        self.display.poll(self.host)
        with patch('tools.display.time.monotonic', return_value=10):
            self.event(KEY_DOWN, 41, 27)
            self.display.poll(self.host)
        with patch('tools.display.time.monotonic', return_value=11):
            self.event(KEY_UP, 41, 27)
            self.display.poll(self.host)
        with patch('tools.display.time.monotonic', return_value=20):
            self.display.poll(self.host)
        self.assertTrue(self.display.captured)
        self.event(KEY_DOWN, 41, 27)
        self.display.poll(self.host)
        self.focus_event(False)
        self.display.poll(self.host)
        self.assertIsNone(self.display.escape_pressed_at)
        self.assertFalse(self.host.pressed_keys)

    def test_activation_click_is_consumed_until_buttons_are_released(self):
        buttons = 1
        def mouse_state(x, y):
            c.cast(x, c.POINTER(c.c_float))[0] = 0
            c.cast(y, c.POINTER(c.c_float))[0] = 0
            return buttons
        with patch.object(self.display.sdl, 'SDL_GetRelativeMouseState', side_effect=mouse_state):
            self.click_event()
            self.display.poll(self.host)
            self.assertTrue(self.display.captured)
            self.assertEqual(self.host.mouse_buttons, 0)
            self.display.poll(self.host)
            self.assertEqual(self.host.mouse_buttons, 0)
            buttons = 0
            self.display.poll(self.host)
            buttons = 1
            self.display.poll(self.host)
            self.assertEqual(self.host.mouse_buttons, 1)

    def test_sdl_keyboard_events_and_focus_loss_release_held_keys(self):
        self.event(KEY_DOWN, 224, 0)
        self.event(KEY_DOWN, 82, 0)
        self.event(KEY_UP, 82, 0)
        self.display.poll(self.host)
        self.assertEqual(bytes(self.host.scan_queue), bytes.fromhex('1de048e0c8'))
        self.focus_event(False)
        self.display.poll(self.host)
        self.assertEqual(bytes(self.host.scan_queue), bytes.fromhex('1de048e0c89d'))
        self.assertFalse(self.host.pressed_keys)
        self.assertEqual(self.host.shift_flags, 0)

    def test_virtual_joystick_axes_buttons_focus_and_hotplug(self):
        sdl = self.display.sdl
        virtual_api(sdl)
        device = attach_virtual(sdl)
        self.assertGreater(device, 0, sdl.SDL_GetError())
        try:
            self.display.joystick_selection = str(device_index(sdl, device))
            self.display.poll_joystick(self.host)
            stick = self.display.joystick
            self.assertTrue(stick)
            values = [-32768, 0, 32767, -12345]
            for axis, value in enumerate(values):
                self.assertTrue(sdl.SDL_SetJoystickVirtualAxis(stick, axis, value))
            self.assertTrue(sdl.SDL_SetJoystickVirtualButton(stick, 0, 1))
            self.assertTrue(sdl.SDL_SetJoystickVirtualButton(stick, 3, 1))
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, values)
            self.assertEqual(self.host.joystick_buttons, 9)
            self.display.verbose = True
            with patch('builtins.print') as printed:
                self.display.poll_joystick(self.host)
                printed.assert_called_once_with(
                    f'input: joystick focus=True; axes={values}; buttons=0x9', flush=True)
                self.display.poll_joystick(self.host)
                self.assertEqual(printed.call_count, 1)
            self.display.verbose = False
            self.display.joystick_axes = [(2, -1), (0, 1)]
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [-32767, -32768])
            self.display.joystick_axes = [(i, 1) for i in range(4)]
            self.display.focused = False
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [0, 0, 0, 0])
            self.assertEqual(self.host.joystick_buttons, 0)
        finally:
            self.assertTrue(sdl.SDL_DetachVirtualJoystick(device))
        self.display.poll_joystick(self.host)
        self.assertEqual(self.host.joystick_axes, [])
        self.assertEqual(self.host.joystick_buttons, 0)

    def test_gamepad_overlay_navigation_repeat_and_button_isolation(self):
        sdl = self.display.sdl
        virtual_api(sdl)
        device = attach_virtual(sdl, axes=2, buttons=8, hats=1)
        self.assertGreater(device, 0)
        try:
            self.display.joystick_selection = str(device_index(sdl, device))
            self.display.poll(self.host)
            stick = self.display.joystick

            def button(index, down):
                self.assertTrue(sdl.SDL_SetJoystickVirtualButton(stick, index, down))
                self.display.poll(self.host)

            button(7, 1)  # Start opens the overlay.
            self.assertTrue(self.display.menu.open)
            self.assertEqual(self.host.joystick_buttons, 0)
            button(7, 0)
            sdl.SDL_SetJoystickVirtualAxis(stick, 1, 25000)
            with patch('tools.display.time.monotonic', return_value=10):
                self.display.poll(self.host)
            self.assertEqual(self.display.menu.selected, 1)
            with patch('tools.display.time.monotonic', return_value=10.1):
                self.display.poll(self.host)
            self.assertEqual(self.display.menu.selected, 1)
            with patch('tools.display.time.monotonic', return_value=10.4):
                self.display.poll(self.host)
            self.assertEqual(self.display.menu.selected, 2)
            sdl.SDL_SetJoystickVirtualAxis(stick, 1, 0)
            self.display.poll(self.host)
            self.display.menu.selected = 4
            button(0, 1)  # A enters Save State.
            self.assertEqual(self.display.menu.page, 'save')
            button(0, 0)
            button(1, 1)  # B returns to main.
            self.assertEqual(self.display.menu.page, 'main')
            button(1, 0)
            button(7, 1)  # Start resumes, without leaking pressed buttons.
            self.assertFalse(self.display.menu.open)
            self.assertEqual(self.host.joystick_buttons, 0)
            button(7, 0)
            button(0, 1)
            self.assertEqual(self.host.joystick_buttons, 1)
            button(0, 0)
        finally:
            sdl.SDL_DetachVirtualJoystick(device)


    def test_second_joystick_selection_and_missing_device(self):
        sdl = self.display.sdl
        virtual_api(sdl)
        devices = [attach_virtual(sdl, axes=2, buttons=2) for _ in range(2)]
        self.assertTrue(all(device > 0 for device in devices))
        try:
            self.display.joystick_selection = str(device_index(sdl, devices[1]))
            self.display.poll_joystick(self.host)
            stick = self.display.joystick
            self.assertTrue(stick)
            sdl.SDL_SetJoystickVirtualAxis(stick, 0, 18000)
            sdl.SDL_SetJoystickVirtualButton(stick, 1, 1)
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [18000, 0])
            self.assertEqual(self.host.joystick_buttons, 2)
            self.assertTrue(sdl.SDL_DetachVirtualJoystick(devices.pop()))
            self.display.poll_joystick(self.host)
            self.assertEqual(self.host.joystick_axes, [])
            self.assertEqual(self.host.joystick_buttons, 0)
        finally:
            for device in reversed(devices):
                sdl.SDL_DetachVirtualJoystick(device)

    def test_supervisor_selects_automatic_controls_before_launch(self):
        from tools.real_mode import run
        self.display.close()
        self.display = None

        class WithVirtualJoystick(Display):
            def __init__(self, **kwargs):
                super().__init__(**kwargs)
                virtual_api(self.sdl)
                self.device = attach_virtual(self.sdl)
                if kwargs.get('joystick') != 'off':
                    self.joystick_selection = str(device_index(self.sdl, self.device))

            def close(self):
                if hasattr(self, 'device'):
                    self.sdl.SDL_DetachVirtualJoystick(self.device)
                    del self.device
                super().close()

        for joystick, expected in (('auto', b'1'), ('off', b'0')):
            with self.subTest(joystick=joystick), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                config = output / 'drive/EF2000/EF2000.CFG'
                config.parent.mkdir(parents=True)
                config.write_bytes(b'CONTROL=9\r\nDETAIL=2\r\nJOY_FACTOR=0\r\n')
                with patch('tools.display.Display', WithVirtualJoystick), (output / 'calls.log').open('w') as log:
                    process, _ = run([sys.executable, '-c', 'pass'], output, log,
                                     headless=False, controls='auto', joystick=joystick)
                self.assertEqual(process.returncode, 0)
                self.assertEqual(config.read_bytes(), b'CONTROL=' + expected + b'\r\nDETAIL=2\r\nJOY_FACTOR=' +
                                 (b'100' if expected == b'1' else b'0') + b'\r\n')
