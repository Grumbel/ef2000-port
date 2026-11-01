#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""SDL window, keyboard, joystick, and mouse input for the original indexed framebuffer."""

import ctypes as c
from ctypes.util import find_library
import os
from pathlib import Path
import time

try:
    from .vga import indexed_rgb
    from .debug_menu import draw_text
except ImportError:
    from vga import indexed_rgb
    from debug_menu import draw_text


class Rect(c.Structure):
    _fields_ = [(name, c.c_int) for name in ('x', 'y', 'w', 'h')]


class FRect(c.Structure):
    _fields_ = [('x', c.c_float), ('y', c.c_float), ('w', c.c_float), ('h', c.c_float)]


class WindowEvent(c.Structure):
    _fields_ = [('type', c.c_uint32), ('reserved', c.c_uint32), ('timestamp', c.c_uint64),
                ('window_id', c.c_uint32), ('data1', c.c_int32), ('data2', c.c_int32)]


class KeyEvent(c.Structure):
    _fields_ = [('type', c.c_uint32), ('reserved', c.c_uint32), ('timestamp', c.c_uint64),
                ('window_id', c.c_uint32), ('which', c.c_uint32), ('scan', c.c_int),
                ('key', c.c_uint32), ('mod', c.c_uint16), ('raw', c.c_uint16),
                ('down', c.c_bool), ('repeat', c.c_bool)]


class MouseButtonEvent(c.Structure):
    _fields_ = [('type', c.c_uint32), ('reserved', c.c_uint32), ('timestamp', c.c_uint64),
                ('window_id', c.c_uint32), ('which', c.c_uint32), ('button', c.c_uint8),
                ('down', c.c_bool), ('clicks', c.c_uint8), ('padding', c.c_uint8),
                ('x', c.c_float), ('y', c.c_float)]


class Event(c.Union):
    _fields_ = [('type', c.c_uint32), ('window', WindowEvent), ('key', KeyEvent),
                ('button', MouseButtonEvent), ('padding', c.c_uint8 * 128)]


# SDL3 event constants, SDL_events.h (SDL 3.2+).
WINDOW_RESIZED = 0x206
WINDOW_FOCUS_GAINED = 0x20e
WINDOW_FOCUS_LOST = 0x20f
WINDOW_CLOSE_REQUESTED = 0x210
KEY_DOWN, KEY_UP = 0x300, 0x301
MOUSE_BUTTON_DOWN = 0x401


ASPECT_RATIOS = ('4:3', 'native', 'stretch')


def startup_rgb():
    """Visible framebuffer before the original BIOS selects a graphics mode."""
    width, height = 640, 400
    pixels = bytearray(width * height * 3)
    draw_text(pixels, width, height, 32, 40, 'EF2000 STARTING', b'\xe0\xe0\xe0', 3)
    draw_text(pixels, width, height, 32, 90, 'WAITING FOR ORIGINAL VIDEO', b'\x90\x90\x90', 2)
    return bytes(pixels)


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
        library = os.environ.get('EF2000_SDL_LIBRARY') or find_library('SDL3')
        if not library:
            raise ValueError('SDL3 unavailable; use the Nix launcher or --headless')
        self.sdl = c.CDLL(library)
        signatures = {
            'SDL_SetAppMetadata': ([c.c_char_p, c.c_char_p, c.c_char_p], c.c_bool),
            'SDL_Init': ([c.c_uint32], c.c_bool),
            'SDL_GetJoysticks': ([c.POINTER(c.c_int)], c.POINTER(c.c_uint32)),
            'SDL_free': ([c.c_void_p], None),
            'SDL_GetJoystickName': ([c.c_void_p], c.c_char_p),
            'SDL_OpenJoystick': ([c.c_uint32], c.c_void_p),
            'SDL_CloseJoystick': ([c.c_void_p], None),
            'SDL_JoystickConnected': ([c.c_void_p], c.c_bool),
            'SDL_GetNumJoystickAxes': ([c.c_void_p], c.c_int),
            'SDL_GetNumJoystickButtons': ([c.c_void_p], c.c_int),
            'SDL_GetJoystickAxis': ([c.c_void_p, c.c_int], c.c_int16),
            'SDL_GetJoystickButton': ([c.c_void_p, c.c_int], c.c_bool),
            'SDL_GetNumJoystickHats': ([c.c_void_p], c.c_int),
            'SDL_GetJoystickHat': ([c.c_void_p, c.c_int], c.c_uint8),
            'SDL_UpdateJoysticks': ([], None),
            'SDL_GetError': ([], c.c_char_p),
            'SDL_IOFromFile': ([c.c_char_p, c.c_char_p], c.c_void_p),
            'SDL_LoadBMP_IO': ([c.c_void_p, c.c_bool], c.c_void_p),
            'SDL_SetWindowIcon': ([c.c_void_p, c.c_void_p], c.c_bool),
            'SDL_DestroySurface': ([c.c_void_p], None),
            'SDL_CreateWindow': ([c.c_char_p, c.c_int, c.c_int, c.c_uint64], c.c_void_p),
            'SDL_SetWindowFullscreen': ([c.c_void_p, c.c_bool], c.c_bool),
            'SDL_GetCurrentVideoDriver': ([], c.c_char_p),
            'SDL_SetWindowTitle': ([c.c_void_p, c.c_char_p], c.c_bool),
            'SDL_CreateRenderer': ([c.c_void_p, c.c_char_p], c.c_void_p),
            'SDL_GetRendererName': ([c.c_void_p], c.c_char_p),
            'SDL_CreateTexture': ([c.c_void_p, c.c_uint32, c.c_int, c.c_int, c.c_int], c.c_void_p),
            'SDL_SetTextureScaleMode': ([c.c_void_p, c.c_int], c.c_bool),
            'SDL_UpdateTexture': ([c.c_void_p, c.c_void_p, c.c_void_p, c.c_int], c.c_bool),
            'SDL_RenderClear': ([c.c_void_p], c.c_bool),
            'SDL_RenderTexture': ([c.c_void_p, c.c_void_p, c.c_void_p, c.POINTER(FRect)], c.c_bool),
            'SDL_RenderPresent': ([c.c_void_p], c.c_bool),
            'SDL_PollEvent': ([c.POINTER(Event)], c.c_bool),
            'SDL_GetWindowSize': ([c.c_void_p, c.POINTER(c.c_int), c.POINTER(c.c_int)], c.c_bool),
            'SDL_SetWindowRelativeMouseMode': ([c.c_void_p, c.c_bool], c.c_bool),
            'SDL_GetRelativeMouseState': ([c.POINTER(c.c_float), c.POINTER(c.c_float)], c.c_uint32),
            'SDL_DestroyTexture': ([c.c_void_p], None),
            'SDL_DestroyRenderer': ([c.c_void_p], None),
            'SDL_DestroyWindow': ([c.c_void_p], None),
            'SDL_Quit': ([], None),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.sdl, name)
            function.argtypes, function.restype = arguments, result
        if not self.sdl.SDL_SetAppMetadata(b'EF2000', os.environ.get('EF2000_VERSION', '0.1.0-dev').encode(), b'ef2000'):
            raise ValueError(self.sdl.SDL_GetError().decode())
        if not self.sdl.SDL_Init(0x220):
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.window = self.sdl.SDL_CreateWindow(b'EF2000 - F10 menu - click to capture mouse',
                                                *geometry, 0x20)
        icon = os.environ.get('EF2000_ICON_BMP') or str(Path(__file__).resolve().parent.parent / 'data/icons/ef2000.bmp')
        if self.window and os.path.isfile(icon):
            stream = self.sdl.SDL_IOFromFile(os.fsencode(icon), b'rb')
            surface = self.sdl.SDL_LoadBMP_IO(stream, True) if stream else None
            if surface:
                self.sdl.SDL_SetWindowIcon(self.window, surface)
                self.sdl.SDL_DestroySurface(surface)
        self.renderer = self.sdl.SDL_CreateRenderer(self.window, None) if self.window else None
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
        self.guest_frame_seen = False
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
        name = self.sdl.SDL_GetRendererName(self.renderer)
        print(f'renderer: {name.decode() if name else "unknown"}', flush=True)
        if fullscreen:
            self.toggle_fullscreen()

    def toggle_fullscreen(self):
        enabled = not self.fullscreen
        if not self.sdl.SDL_SetWindowFullscreen(self.window, enabled):
            print(f'window: fullscreen failed: {self.sdl.SDL_GetError().decode()}', flush=True)
            return
        self.fullscreen = enabled
        print(f'window: fullscreen={enabled}', flush=True)

    def cycle_aspect_ratio(self):
        self.aspect_ratio = ASPECT_RATIOS[(ASPECT_RATIOS.index(self.aspect_ratio) + 1) % len(ASPECT_RATIOS)]
        print(f'video: aspect ratio={self.aspect_ratio}', flush=True)

    def toggle_smooth_scaling(self):
        self.smooth_scaling = not self.smooth_scaling
        if self.texture and not self.sdl.SDL_SetTextureScaleMode(self.texture, int(self.smooth_scaling)):
            raise ValueError(self.sdl.SDL_GetError().decode())
        print(f'video: smooth scaling={self.smooth_scaling}', flush=True)

    def capture(self, enabled):
        if not self.sdl.SDL_SetWindowRelativeMouseMode(self.window, enabled):
            raise ValueError(self.sdl.SDL_GetError().decode())
        self.captured = enabled
        self.mouse_armed = False
        title = (b'EF2000 - F10 menu - hold Esc for 3s to release mouse' if enabled
                 else b'EF2000 - F10 menu - click to capture mouse')
        self.sdl.SDL_SetWindowTitle(self.window, title)
        self.motion_remainder = [0.0, 0.0]
        x, y = c.c_float(), c.c_float()
        self.sdl.SDL_GetRelativeMouseState(c.byref(x), c.byref(y))

    def poll(self, engine):
        event = Event()
        while self.sdl.SDL_PollEvent(c.byref(event)):
            kind = event.type
            if kind in (0x100, WINDOW_CLOSE_REQUESTED):
                self.closed = True
            elif kind == WINDOW_RESIZED:
                print(f'window: size={event.window.data1}x{event.window.data2}', flush=True)
            elif kind == WINDOW_FOCUS_GAINED:
                self.focused = True
            elif kind == WINDOW_FOCUS_LOST:
                self.focused = False
                self.escape_pressed_at = None
                engine.release_keys()
                self.capture(False)
            elif kind == MOUSE_BUTTON_DOWN and event.button.button == 1 and self.menu.open:
                self.menu.click(event.button.x, event.button.y)
            elif kind == MOUSE_BUTTON_DOWN and event.button.button == 1 and not self.captured:
                x, y = event.button.x, event.button.y
                if (self.rect.x <= x < self.rect.x + self.rect.w and
                        self.rect.y <= y < self.rect.y + self.rect.h):
                    self.capture(True)
            elif kind in (KEY_DOWN, KEY_UP):
                scan, key = event.key.scan, event.key.key
                down = kind == KEY_DOWN
                if scan == 67:  # F10 is reserved for the host overlay.
                    if down and not event.key.repeat:
                        self.menu.toggle(engine)
                    continue
                if scan == 68:  # F11
                    if down and not event.key.repeat:
                        self.toggle_fullscreen()
                    continue
                if self.menu.open:
                    if down:
                        self.menu.key(scan)
                    continue
                if scan == 41:
                    if kind == 0x301:
                        self.escape_pressed_at = None
                    elif not event.key.repeat and self.escape_pressed_at is None:
                        self.escape_pressed_at = time.monotonic()
                engine.key_event(scan, key, down, repeat=event.key.repeat)
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
        x, y = c.c_float(), c.c_float()
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
        if self.joystick and not self.sdl.SDL_JoystickConnected(self.joystick):
            self.sdl.SDL_CloseJoystick(self.joystick)
            self.joystick = None
            print('input: joystick disconnected', flush=True)
        index = 0 if self.joystick_selection == 'auto' else (
            int(self.joystick_selection) if self.joystick_selection != 'off' else -1)
        if not self.joystick and index >= 0:
            count = c.c_int()
            devices = self.sdl.SDL_GetJoysticks(c.byref(count))
            try:
                if devices and index < count.value:
                    self.joystick = self.sdl.SDL_OpenJoystick(devices[index])
                    if self.joystick:
                        name = self.sdl.SDL_GetJoystickName(self.joystick)
                        print(f'input: joystick {index}: {name.decode() if name else "unknown"}', flush=True)
                    else:
                        print(f'input: joystick {index} unavailable: {self.sdl.SDL_GetError().decode()}', flush=True)
            finally:
                if devices:
                    self.sdl.SDL_free(devices)
        engine.joystick_axes = []
        engine.joystick_buttons = 0
        if self.joystick:
            self.sdl.SDL_UpdateJoysticks()
            count = self.sdl.SDL_GetNumJoystickAxes(self.joystick)
            for axis, sign in self.joystick_axes:
                if axis >= count:
                    break
                value = self.sdl.SDL_GetJoystickAxis(self.joystick, axis) if self.focused else 0
                engine.joystick_axes.append(max(-32768, min(32767, value * sign)))
            if self.focused:
                engine.joystick_buttons = sum(
                    bool(self.sdl.SDL_GetJoystickButton(self.joystick, button)) << button
                    for button in range(min(4, self.sdl.SDL_GetNumJoystickButtons(self.joystick))))

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
            count = self.sdl.SDL_GetNumJoystickButtons(self.joystick)
            buttons = sum(1 << button for button in (0, 1, 7)
                          if button < count and self.sdl.SDL_GetJoystickButton(self.joystick, button))
            if self.sdl.SDL_GetNumJoystickHats(self.joystick):
                hat = self.sdl.SDL_GetJoystickHat(self.joystick, 0)
                direction = -1 if hat & 1 else (1 if hat & 4 else 0)
            if not direction and self.sdl.SDL_GetNumJoystickAxes(self.joystick) > 1:
                axis = self.sdl.SDL_GetJoystickAxis(self.joystick, 1)
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
            width, height = 640, 400
            pixels = startup_rgb()
        else:
            width, height, indices = frame
            pixels = indexed_rgb(indices, memory[0xf1000:0xf1300])
            if not self.guest_frame_seen and any(pixels):
                self.guest_frame_seen = True
                print(f'graphics: first nonblack original frame {width}x{height}', flush=True)
            elif not self.guest_frame_seen:
                # Video mode 13h is selected at ~11M guest instructions but
                # the splash is still entirely black at 30M. Keep informing
                # the user that emulation is making progress until pixels
                # from the original game actually become visible.
                width, height = 640, 400
                pixels = startup_rgb()
        self.last_frame = now
        if self.dimensions != (width, height):
            if self.texture:
                self.sdl.SDL_DestroyTexture(self.texture)
            self.texture = self.sdl.SDL_CreateTexture(self.renderer, 0x17101803, 1, width, height)
            if not self.texture:
                raise ValueError(self.sdl.SDL_GetError().decode())
            if not self.sdl.SDL_SetTextureScaleMode(self.texture, int(self.smooth_scaling)):
                raise ValueError(self.sdl.SDL_GetError().decode())
            self.dimensions = (width, height)
            print(f'graphics: framebuffer={width}x{height}; indexed 8-bit', flush=True)
        if self.menu.open:
            pixels = self.menu.render(pixels, width, height)
        window_width, window_height = c.c_int(), c.c_int()
        self.sdl.SDL_GetWindowSize(self.window, c.byref(window_width), c.byref(window_height))
        self.rect = viewport_rect(window_width.value, window_height.value, width, height, self.aspect_ratio)
        if not self.sdl.SDL_UpdateTexture(self.texture, None, c.c_char_p(pixels), width * 3):
            raise ValueError(self.sdl.SDL_GetError().decode())
        if not self.sdl.SDL_RenderClear(self.renderer):
            raise ValueError(self.sdl.SDL_GetError().decode())
        target = FRect(self.rect.x, self.rect.y, self.rect.w, self.rect.h)
        if not self.sdl.SDL_RenderTexture(self.renderer, self.texture, None, c.byref(target)):
            raise ValueError(self.sdl.SDL_GetError().decode())
        if not self.sdl.SDL_RenderPresent(self.renderer):
            raise ValueError(self.sdl.SDL_GetError().decode())

    def close(self):
        if self.joystick:
            self.sdl.SDL_CloseJoystick(self.joystick)
            self.joystick = None
        if self.texture:
            self.sdl.SDL_DestroyTexture(self.texture)
        if self.renderer:
            self.sdl.SDL_DestroyRenderer(self.renderer)
        if self.window:
            self.sdl.SDL_DestroyWindow(self.window)
        self.sdl.SDL_Quit()
