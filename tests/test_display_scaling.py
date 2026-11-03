# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import unittest
from types import SimpleNamespace
from unittest.mock import patch

from tools.display import Display, startup_rgb, viewport_rect


class DisplayScalingTests(unittest.TestCase):
    def test_startup_frame_is_visible_before_guest_selects_video_mode(self):
        pixels = startup_rgb()
        self.assertEqual(len(pixels), 640 * 400 * 3)
        self.assertEqual(pixels[:3], b'\0\0\0')
        self.assertIn(b'\xe0\xe0\xe0', pixels)

    def test_black_guest_frame_keeps_startup_visible_until_first_colored_frame(self):
        presented = []
        display = Display.__new__(Display)
        display.sdl = SimpleNamespace(
            SDL_DestroyTexture=lambda _: None,
            SDL_CreateTexture=lambda *args: 1,
            SDL_SetTextureScaleMode=lambda *args: True,
            SDL_GetWindowSize=lambda window, width, height: (
                setattr(width._obj, 'value', 1280), setattr(height._obj, 'value', 960)),
            SDL_UpdateTexture=lambda *args: True,
            SDL_RenderClear=lambda *args: True,
            SDL_RenderTexture=lambda *args: True,
            SDL_RenderPresent=lambda renderer: presented.append(renderer) or True,
        )
        display.renderer = 10
        display.window = 11
        display.poll = lambda engine: None
        display.last_frame = 0
        display.guest_frame_seen = False
        display.texture = None
        display.dimensions = None
        display.smooth_scaling = False
        display.aspect_ratio = '4:3'
        display.menu = SimpleNamespace(open=False)
        engine = SimpleNamespace(frame=lambda memory: (320, 200, bytes(320 * 200)))
        memory = bytearray(0x100000)
        with patch('tools.display.time.monotonic', side_effect=[1, 2]):
            display.update(engine, memory)
            self.assertFalse(display.guest_frame_seen)
            self.assertEqual(display.dimensions, (640, 400))
            memory[0xf1000 + 3] = 63
            engine.frame = lambda memory: (320, 200, bytes([1]) * (320 * 200))
            display.update(engine, memory)
        self.assertTrue(display.guest_frame_seen)
        self.assertEqual(display.dimensions, (320, 200))
        self.assertEqual(presented, [10, 10])

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
