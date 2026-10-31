#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""SDL window, keyboard, joystick, and mouse input for the original indexed framebuffer."""

import ctypes as c
from ctypes.util import find_library
import os
import struct
import time

try:
    from .vga import indexed_rgb
except ImportError:
    from vga import indexed_rgb


class Rect(c.Structure):
    _fields_ = [(name, c.c_int) for name in ('x', 'y', 'w', 'h')]


class RendererInfo(c.Structure):
    _fields_ = [('name', c.c_char_p), ('flags', c.c_uint32),
                ('count', c.c_uint32), ('formats', c.c_uint32 * 16),
                ('max_width', c.c_int), ('max_height', c.c_int)]


class Display:
    def __init__(self, joystick='auto', joystick_axes=None, states=None, fullscreen=False):
        library = os.environ.get('EF2000_SDL_LIBRARY') or find_library('SDL2')
        if not library:
            raise ValueError('SDL2 unavailable; use the Nix launcher or --headless')
        self.sdl = c.CDLL(library)
        signatures = {
            'SDL_Init': ([c.c_uint32], c.c_int),
            'SDL_NumJoysticks': ([], c.c_int),
            'SDL_JoystickName': ([c.c_void_p], c.c_char_p),
            'SDL_GetRendererInfo': ([c.c_void_p, c.c_void_p], c.c_int),
            'SDL_JoystickOpen': ([c.c_int], c.c_void_p),
            'SDL_JoystickClose': ([c.c_void_p], None),
            'SDL_JoystickGetAttached': ([c.c_void_p], c.c_int),
            'SDL_JoystickNumAxes': ([c.c_void_p], c.c_int),
            'SDL_JoystickNumButtons': ([c.c_void_p], c.c_int),
            'SDL_JoystickGetAxis': ([c.c_void_p, c.c_int], c.c_int16),
            'SDL_JoystickGetButton': ([c.c_void_p, c.c_int], c.c_uint8),
            'SDL_JoystickUpdate': ([], None),
            'SDL_GetError': ([], c.c_char_p),
            'SDL_CreateWindow': ([c.c_char_p, c.c_int, c.c_int, c.c_int, c.c_int, c.c_uint32], c.c_void_p),
            'SDL_SetWindowFullscreen': ([c.c_void_p, c.c_uint32], c.c_int),
            'SDL_GetCurrentVideoDriver': ([], c.c_char_p),
            'SDL_SetWindowTitle': ([c.c_void_p, c.c_char_p], None),
            'SDL_CreateRenderer': ([c.c_void_p, c.c_int, c.c_uint32], c.c_void_p),
            'SDL_CreateTexture': ([c.c_void_p, c.c_uint32, c.c_int, c.c_int, c.c_int], c.c_void_p),
            'SDL_UpdateTexture': ([c.c_void_p, c.c_void_p, c.c_void_p, c.c_int], c.c_int),
            'SDL_RenderClear': ([c.c_void_p], c.c_int),
            'SDL_RenderCopy': ([c.c_void_p, c.c_void_p, c.c_void_p, c.POINTER(Rect)], c.c_int),
            'SDL_RenderPresent': ([c.c_void_p], None),
            'SDL_PollEvent': ([c.c_void_p], c.c_int),
            'SDL_GetWindowSize': ([c.c_void_p, c.POINTER(c.c_int), c.POINTER(c.c_int)], None),
            'SDL_SetRelativeMouseMode': ([c.c_int], c.c_int),
            'SDL_GetRelativeMouseState': ([c.POINTER(c.c_int), c.POINTER(c.c_int)], c.c_uint32),
            'SDL_GetMouseState': ([c.POINTER(c.c_int), c.POINTER(c.c_int)], c.c_uint32),
            'SDL_DestroyTexture': ([c.c_void_p], None),
            'SDL_DestroyRenderer': ([c.c_void_p], None),
            'SDL_DestroyWindow': ([c.c_void_p], None),
            'SDL_Quit': ([], None),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.sdl, name)
            function.argtypes, function.restype = arguments, result
        if self.sdl.SDL_Init(0x220) < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.window = self.sdl.SDL_CreateWindow(b'EF2000 - F10 menu - click to capture mouse', 0x2fff0000, 0x2fff0000, 960, 720, 0x24)
        self.renderer = self.sdl.SDL_CreateRenderer(self.window, -1, 0) if self.window else None
        self.texture = None
        self.dimensions = None
        self.fullscreen = False
        try:
            from .debug_menu import DebugMenu
        except ImportError:
            from debug_menu import DebugMenu
        self.menu = DebugMenu(self, states)
        self.closed = False
        self.last_frame = 0
        self.joystick = None
        self.joystick_selection = joystick
        self.joystick_axes = joystick_axes or [(i, 1) for i in range(4)]
        self.focused = False
        self.captured = False
        self.escape_pressed_at = None
        self.mouse_armed = False
        self.motion_remainder = [0.0, 0.0]
        self.rect = Rect(0, 0, 960, 720)
        if not self.renderer:
            error = self.sdl.SDL_GetError().decode()
            self.close()
            raise ValueError(error)

        print(f'video: SDL driver={self.sdl.SDL_GetCurrentVideoDriver().decode()}; window=960x720', flush=True)
        info = RendererInfo()
        if self.sdl.SDL_GetRendererInfo(self.renderer, c.byref(info)) == 0:
            print(f'renderer: {info.name.decode()}; flags=0x{info.flags:x}; max texture={info.max_width}x{info.max_height}', flush=True)
        if fullscreen:
            self.toggle_fullscreen()

    def toggle_fullscreen(self):
        enabled = not self.fullscreen
        if self.sdl.SDL_SetWindowFullscreen(self.window, 0x1001 if enabled else 0) < 0:
            print(f'window: fullscreen failed: {self.sdl.SDL_GetError().decode()}', flush=True)
            return
        self.fullscreen = enabled
        print(f'window: fullscreen={enabled}', flush=True)

    def capture(self, enabled):
        if self.sdl.SDL_SetRelativeMouseMode(int(enabled)) < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.captured = enabled
        self.mouse_armed = False
        title = (b'EF2000 - F10 menu - hold Esc for 3s to release mouse' if enabled
                 else b'EF2000 - F10 menu - click to capture mouse')
        self.sdl.SDL_SetWindowTitle(self.window, title)
        self.motion_remainder = [0.0, 0.0]
        x, y = c.c_int(), c.c_int()
        self.sdl.SDL_GetRelativeMouseState(c.byref(x), c.byref(y))

    def poll(self, engine):
        event = (c.c_ubyte * 64)()
        while self.sdl.SDL_PollEvent(c.byref(event)):
            data = bytes(event)
            kind = struct.unpack_from('<I', data)[0]
            if kind == 0x100:
                self.closed = True
            elif kind == 0x200 and data[12] == 6:
                width, height = struct.unpack_from('<ii', data, 16)
                print(f'window: size={width}x{height}', flush=True)
            elif kind == 0x200 and data[12] == 12:
                self.focused = True
            elif kind == 0x200 and data[12] == 13:
                self.focused = False
                self.escape_pressed_at = None
                engine.release_keys()
                self.capture(False)
            elif kind == 0x401 and data[16] == 1 and self.menu.open:
                self.menu.click(*struct.unpack_from('<ii', data, 20))
            elif kind == 0x401 and data[16] == 1 and not self.captured:
                x, y = struct.unpack_from('<ii', data, 20)
                if (self.rect.x <= x < self.rect.x + self.rect.w and
                        self.rect.y <= y < self.rect.y + self.rect.h):
                    self.capture(True)
            elif kind in (0x300, 0x301):
                scan, key = struct.unpack_from('<ii', data, 16)
                down = kind == 0x300
                if scan == 67:  # F10 is reserved for the host overlay.
                    if down and not data[13]:
                        self.menu.toggle(engine)
                    continue
                if scan == 68:  # F11
                    if down and not data[13]:
                        self.toggle_fullscreen()
                    continue
                if self.menu.open:
                    if down:
                        self.menu.key(scan)
                    continue
                if scan == 41:
                    if kind == 0x301:
                        self.escape_pressed_at = None
                    elif not data[13] and self.escape_pressed_at is None:
                        self.escape_pressed_at = time.monotonic()
                engine.key_event(scan, key, kind == 0x300, repeat=bool(data[13]))
        if (self.captured and self.escape_pressed_at is not None and
                time.monotonic() - self.escape_pressed_at >= 3):
            self.capture(False)
            self.escape_pressed_at = None
        if self.menu.open:
            engine.mouse_buttons = 0
            engine.joystick_buttons = 0
            engine.joystick_axes = []
            return
        self.poll_joystick(engine)
        x, y = c.c_int(), c.c_int()
        buttons = self.sdl.SDL_GetRelativeMouseState(c.byref(x), c.byref(y))
        if self.captured:
            width, height = self.dimensions or (640, 400)
            deltas = []
            for axis, movement, extent, viewport in (
                    (0, x.value, width, self.rect.w),
                    (1, y.value, height, self.rect.h)):
                total = self.motion_remainder[axis] + movement * extent / max(1, viewport)
                delta = int(total)
                self.motion_remainder[axis] = total - delta
                deltas.append(delta)
            engine.move_mouse(*deltas)
            # A click acquires the pointer; its held buttons must be released
            # before a subsequent click can activate anything in the game.
            if not buttons:
                self.mouse_armed = True
            engine.mouse_buttons = ((buttons & 1) | ((buttons & 4) >> 1) |
                                    ((buttons & 2) << 1)) if self.mouse_armed else 0
        else:
            engine.mouse_buttons = 0

    def poll_joystick(self, engine):
        if self.joystick and not self.sdl.SDL_JoystickGetAttached(self.joystick):
            self.sdl.SDL_JoystickClose(self.joystick)
            self.joystick = None
            print('input: joystick disconnected', flush=True)
        index = 0 if self.joystick_selection == 'auto' else (
            int(self.joystick_selection) if self.joystick_selection != 'off' else -1)
        if not self.joystick and 0 <= index < self.sdl.SDL_NumJoysticks():
            self.joystick = self.sdl.SDL_JoystickOpen(index)
            if self.joystick:
                print(f'input: joystick {index}: {self.sdl.SDL_JoystickName(self.joystick).decode()}', flush=True)
            else:
                print(f'input: joystick {index} unavailable: {self.sdl.SDL_GetError().decode()}', flush=True)
        engine.joystick_axes = []
        engine.joystick_buttons = 0
        if self.joystick:
            self.sdl.SDL_JoystickUpdate()
            count = self.sdl.SDL_JoystickNumAxes(self.joystick)
            for axis, sign in self.joystick_axes:
                if axis >= count:
                    break
                value = self.sdl.SDL_JoystickGetAxis(self.joystick, axis) if self.focused else 0
                engine.joystick_axes.append(max(-32768, min(32767, value * sign)))
            if self.focused:
                engine.joystick_buttons = sum(
                    bool(self.sdl.SDL_JoystickGetButton(self.joystick, button)) << button
                    for button in range(min(4, self.sdl.SDL_JoystickNumButtons(self.joystick))))

    def update(self, engine, memory):
        self.poll(engine)
        now = time.monotonic()
        if now - self.last_frame < 1 / 30:
            return
        frame = engine.frame(memory)
        if frame is None:
            if not self.menu.open:
                return
            frame = (640, 400, bytes(640 * 400))
        width, height, indices = frame
        self.last_frame = now
        if self.dimensions != (width, height):
            if self.texture:
                self.sdl.SDL_DestroyTexture(self.texture)
            self.texture = self.sdl.SDL_CreateTexture(self.renderer, 0x17101803, 1, width, height)
            if not self.texture:
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.dimensions = (width, height)
            print(f'graphics: framebuffer={width}x{height}; indexed 8-bit', flush=True)
        pixels = indexed_rgb(indices, memory[0xf1000:0xf1300])
        if self.menu.open:
            pixels = self.menu.render(pixels, width, height)
        window_width, window_height = c.c_int(), c.c_int()
        self.sdl.SDL_GetWindowSize(self.window, c.byref(window_width), c.byref(window_height))
        render_width = min(window_width.value, window_height.value * 4 // 3)
        render_height = render_width * 3 // 4
        self.rect = Rect((window_width.value - render_width) // 2,
                         (window_height.value - render_height) // 2, render_width, render_height)
        if self.sdl.SDL_UpdateTexture(self.texture, None, c.c_char_p(pixels), width * 3) < 0:
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.sdl.SDL_RenderClear(self.renderer)
        self.sdl.SDL_RenderCopy(self.renderer, self.texture, None, c.byref(self.rect))
        self.sdl.SDL_RenderPresent(self.renderer)

    def close(self):
        if self.joystick:
            self.sdl.SDL_JoystickClose(self.joystick)
            self.joystick = None
        if self.texture:
            self.sdl.SDL_DestroyTexture(self.texture)
        if self.renderer:
            self.sdl.SDL_DestroyRenderer(self.renderer)
        if self.window:
            self.sdl.SDL_DestroyWindow(self.window)
        self.sdl.SDL_Quit()
