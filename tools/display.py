#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""SDL window, keyboard, joystick, and mouse input for the original indexed framebuffer."""

import ctypes as c
from ctypes.util import find_library
import os
from pathlib import Path
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


ASPECT_RATIOS = ('4:3', 'native', 'stretch')


def viewport_rect(window_width, window_height, frame_width, frame_height, aspect_ratio='4:3'):
    """Compute the destination viewport for CRT, square-pixel or stretched output."""
    if aspect_ratio == 'stretch':
        return Rect(0, 0, window_width, window_height)
    if aspect_ratio == '4:3':
        ratio_width, ratio_height = 4, 3
    elif aspect_ratio == 'native':
        ratio_width, ratio_height = frame_width, frame_height
    else:
        raise ValueError(f'invalid aspect ratio: {aspect_ratio}')
    width = min(window_width, window_height * ratio_width // ratio_height)
    height = min(window_height, window_width * ratio_height // ratio_width)
    return Rect((window_width - width) // 2, (window_height - height) // 2, width, height)


class Display:
    def __init__(self, joystick='auto', joystick_axes=None, states=None, fullscreen=False,
                 geometry=(1280, 960), aspect_ratio='4:3', smooth_scaling=False):
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
            'SDL_JoystickNumHats': ([c.c_void_p], c.c_int),
            'SDL_JoystickGetHat': ([c.c_void_p, c.c_int], c.c_uint8),
            'SDL_JoystickUpdate': ([], None),
            'SDL_GetError': ([], c.c_char_p),
            'SDL_SetHint': ([c.c_char_p, c.c_char_p], c.c_int),
            'SDL_RWFromFile': ([c.c_char_p, c.c_char_p], c.c_void_p),
            'SDL_LoadBMP_RW': ([c.c_void_p, c.c_int], c.c_void_p),
            'SDL_SetWindowIcon': ([c.c_void_p, c.c_void_p], None),
            'SDL_FreeSurface': ([c.c_void_p], None),
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
        for name, value in ((b'SDL_VIDEO_X11_WMCLASS', b'ef2000'),
                            (b'SDL_VIDEO_WAYLAND_WMCLASS', b'ef2000'),
                            (b'SDL_APP_NAME', b'EF2000')):
            self.sdl.SDL_SetHint(name, value)
        self.window = self.sdl.SDL_CreateWindow(b'EF2000 - F10 menu - click to capture mouse',
                                                0x2fff0000, 0x2fff0000, *geometry, 0x24)
        icon = os.environ.get('EF2000_ICON_BMP') or str(Path(__file__).resolve().parent.parent / 'data/icons/ef2000.bmp')
        if self.window and os.path.isfile(icon):
            stream = self.sdl.SDL_RWFromFile(os.fsencode(icon), b'rb')
            surface = self.sdl.SDL_LoadBMP_RW(stream, 1) if stream else None
            if surface:
                self.sdl.SDL_SetWindowIcon(self.window, surface)
                self.sdl.SDL_FreeSurface(surface)
        self.renderer = self.sdl.SDL_CreateRenderer(self.window, -1, 0) if self.window else None
        self.texture = None
        self.dimensions = None
        self.fullscreen = False
        if aspect_ratio not in ASPECT_RATIOS:
            raise ValueError(f'invalid aspect ratio: {aspect_ratio}')
        self.aspect_ratio = aspect_ratio
        self.smooth_scaling = smooth_scaling
        try:
            from .debug_menu import DebugMenu
        except ImportError:
            from debug_menu import DebugMenu
        self.menu = DebugMenu(self, states)
        self.closed = False
        self.last_frame = 0
        self.joystick = None
        self.verbose = False
        self.last_joystick_trace = None
        self.next_joystick_trace = 0
        self.joystick_selection = joystick
        self.joystick_axes = joystick_axes or [(i, 1) for i in range(4)]
        self.menu_buttons = 0
        self.menu_direction = 0
        self.menu_repeat_at = 0
        self.menu_release_pending = False
        self.focused = False
        self.captured = False
        self.escape_pressed_at = None
        self.mouse_armed = False
        self.motion_remainder = [0.0, 0.0]
        self.rect = Rect(0, 0, *geometry)
        if not self.renderer:
            error = self.sdl.SDL_GetError().decode()
            self.close()
            raise ValueError(error)

        print(f'video: SDL driver={self.sdl.SDL_GetCurrentVideoDriver().decode()}; window={geometry[0]}x{geometry[1]}', flush=True)
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

    def cycle_aspect_ratio(self):
        self.aspect_ratio = ASPECT_RATIOS[(ASPECT_RATIOS.index(self.aspect_ratio) + 1) % len(ASPECT_RATIOS)]
        print(f'video: aspect ratio={self.aspect_ratio}', flush=True)

    def toggle_smooth_scaling(self):
        self.smooth_scaling = not self.smooth_scaling
        # SDL's scale-quality hint is applied when the texture is created.
        if self.texture:
            self.sdl.SDL_DestroyTexture(self.texture)
            self.texture = None
            self.dimensions = None
        print(f'video: smooth scaling={self.smooth_scaling}', flush=True)

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
        self.poll_joystick(engine)
        self.poll_menu_joystick(engine)
        if self.menu.open:
            engine.mouse_buttons = 0
            engine.joystick_buttons = 0
            engine.joystick_axes = []
            return
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

        sample = (self.focused, tuple(engine.joystick_axes), engine.joystick_buttons)
        now = time.monotonic()
        if self.verbose and sample != self.last_joystick_trace and now >= self.next_joystick_trace:
            print(f'input: joystick focus={self.focused}; axes={engine.joystick_axes}; '
                  f'buttons=0x{engine.joystick_buttons:x}', flush=True)
            self.last_joystick_trace = sample
            self.next_joystick_trace = now + 0.25

    def poll_menu_joystick(self, engine):
        """Use the selected SDL joystick for the host overlay, independent of DOS mapping."""
        buttons = 0
        direction = 0
        if self.joystick and self.focused:
            count = self.sdl.SDL_JoystickNumButtons(self.joystick)
            buttons = sum(1 << button for button in (0, 1, 7)
                          if button < count and self.sdl.SDL_JoystickGetButton(self.joystick, button))
            if self.sdl.SDL_JoystickNumHats(self.joystick):
                hat = self.sdl.SDL_JoystickGetHat(self.joystick, 0)
                direction = -1 if hat & 1 else (1 if hat & 4 else 0)
            if not direction and self.sdl.SDL_JoystickNumAxes(self.joystick) > 1:
                axis = self.sdl.SDL_JoystickGetAxis(self.joystick, 1)
                direction = -1 if axis < -16000 else (1 if axis > 16000 else 0)

        pressed = buttons & ~self.menu_buttons
        self.menu_buttons = buttons
        if pressed & (1 << 7):
            self.menu.toggle(engine)
            self.menu_release_pending = True
        if self.menu.open:
            if pressed & (1 << 1):
                self.menu.key(41)
            elif pressed & 1:
                self.menu.key(40)
            now = time.monotonic()
            if direction and (direction != self.menu_direction or now >= self.menu_repeat_at):
                self.menu.key(81 if direction > 0 else 82)
                self.menu_repeat_at = now + (0.30 if direction != self.menu_direction else 0.12)
            self.menu_release_pending = True
        self.menu_direction = direction if self.menu.open else 0
        if self.menu_release_pending:
            engine.joystick_buttons = 0
            if not buttons:
                self.menu_release_pending = False

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
            self.sdl.SDL_SetHint(b'SDL_RENDER_SCALE_QUALITY', b'linear' if self.smooth_scaling else b'nearest')
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
        self.rect = viewport_rect(window_width.value, window_height.value, width, height, self.aspect_ratio)
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
