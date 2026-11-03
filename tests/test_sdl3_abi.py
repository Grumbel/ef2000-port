# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import ctypes as c
import unittest

from tools.audio_output import AudioSpec
from tools.display import (Event, KeyEvent, MouseButtonEvent, WindowEvent,
                           KEY_DOWN, MOUSE_BUTTON_DOWN, WINDOW_FOCUS_LOST)
from tests.test_sdl_input import VirtualJoystickDesc


class SDL3ABITests(unittest.TestCase):
    def test_event_layout_matches_sdl3_2_headers(self):
        self.assertEqual(c.sizeof(Event), 128)
        self.assertEqual(c.sizeof(KeyEvent), 40)
        self.assertEqual(c.sizeof(WindowEvent), 32)
        self.assertEqual(c.sizeof(MouseButtonEvent), 40)
        self.assertEqual(KeyEvent.scan.offset, 24)
        self.assertEqual(KeyEvent.repeat.offset, 37)
        self.assertEqual(MouseButtonEvent.x.offset, 28)

    def test_typed_events(self):
        event = Event()
        event.key.type, event.key.scan, event.key.repeat = KEY_DOWN, 67, True
        self.assertEqual(event.type, KEY_DOWN)
        self.assertEqual(event.key.scan, 67)
        self.assertTrue(event.key.repeat)
        event.button.type, event.button.x = MOUSE_BUTTON_DOWN, 234.5
        self.assertAlmostEqual(event.button.x, 234.5)
        event.window.type = WINDOW_FOCUS_LOST
        self.assertEqual(event.type, WINDOW_FOCUS_LOST)

    def test_audio_and_virtual_joystick_specs(self):
        self.assertEqual(c.sizeof(AudioSpec), 12)
        self.assertEqual((AudioSpec.format.offset, AudioSpec.channels.offset,
                          AudioSpec.freq.offset), (0, 4, 8))
        if c.sizeof(c.c_void_p) == 8:
            self.assertEqual(c.sizeof(VirtualJoystickDesc), 136)
