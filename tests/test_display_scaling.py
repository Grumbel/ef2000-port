# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import unittest
from types import SimpleNamespace

from tools.display import Display, viewport_rect


class DisplayScalingTests(unittest.TestCase):
    def test_crt_aspect_correction(self):
        rect = viewport_rect(1280, 960, 640, 400)
        self.assertEqual((rect.x, rect.y, rect.w, rect.h), (0, 0, 1280, 960))
        rect = viewport_rect(1280, 720, 640, 400)
        self.assertEqual((rect.x, rect.y, rect.w, rect.h), (160, 0, 960, 720))

    def test_native_square_pixels_and_stretch(self):
        rect = viewport_rect(1280, 960, 640, 400, 'native')
        self.assertEqual((rect.x, rect.y, rect.w, rect.h), (0, 80, 1280, 800))
        rect = viewport_rect(1280, 720, 640, 400, 'stretch')
        self.assertEqual((rect.x, rect.y, rect.w, rect.h), (0, 0, 1280, 720))

    def test_invalid_mode(self):
        with self.assertRaises(ValueError):
            viewport_rect(1280, 960, 640, 400, 'invalid')

    def test_live_menu_toggles_and_texture_recreation(self):
        scale_modes = []
        display = Display.__new__(Display)
        display.sdl = SimpleNamespace(SDL_SetTextureScaleMode=lambda texture, mode: scale_modes.append((texture, mode)) or True)
        display.texture = 123
        display.dimensions = (640, 400)
        display.smooth_scaling = False
        display.aspect_ratio = '4:3'
        display.toggle_smooth_scaling()
        self.assertTrue(display.smooth_scaling)
        self.assertEqual(scale_modes, [(123, 1)])
        self.assertEqual(display.texture, 123)
        self.assertEqual(display.dimensions, (640, 400))
        for expected in ('native', 'stretch', '4:3'):
            display.cycle_aspect_ratio()
            self.assertEqual(display.aspect_ratio, expected)
