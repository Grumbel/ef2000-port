# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Small keyboard and mouse operated debug overlay, rendered into the RGB frame."""

# Five by seven bitmap font, kept independent of system fonts and SDL_ttf.
FONT = dict(zip('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./:()>_', (
'0E11111F111111','1E11111E11111E','0F10101010100F','1E11111111111E','1F10101E10101F',
'1F10101E101010','0F10101711110F','1111111F111111','0E04040404040E','0702020212120C',
'11121418141211','1010101010101F','111B1515111111','11191513111111','0E11111111110E',
'1E11111E101010','0E11111115120D','1E11111E141211','0F10100E01011E','1F040404040404',
'1111111111110E','11111111110A04','11111115151B11','11110A040A1111','11110A04040404',
'1F01020408101F','0E11131519110E','040C040404040E','0E11010204081F','1E01010601011E',
'02060A121F0202','1F101E0101110E','06101E1111110E','1F010204080808','0E11110E11110E',
'0E11110F01010C','0000001F000000','00000000000C0C','01010204081010','000C0C000C0C00',
'02040808080402','08040202020408','10080402040810','0000000000001F')))


def draw_text(pixels, width, height, x, y, text, color, scale=2):
    for letter in text.upper():
        rows = FONT.get(letter, '00000000000000')
        for row in range(7):
            bits = int(rows[row * 2:row * 2 + 2], 16)
            for col in range(5):
                if bits & (16 >> col):
                    for dy in range(scale):
                        for dx in range(scale):
                            xx, yy = x + col * scale + dx, y + row * scale + dy
                            if 0 <= xx < width and 0 <= yy < height:
                                index = (yy * width + xx) * 3
                                pixels[index:index + 3] = color
        x += 6 * scale


class DebugMenu:
    def __init__(self, display, states):
        self.display, self.states = display, states
        self.open = False
        self.page = 'main'
        self.selected = 0
        self.scroll = 0
        self.status = '' if states else 'Save states require native64 and sound off'

    def rows(self):
        if self.page == 'main':
            return ['Resume', f'Fullscreen: {"On" if self.display.fullscreen else "Off"}',
                    'Save State >', 'Load State >', 'Quit']
        entries = [self.states.label(path) for path in self.states.entries()] if self.states else []
        return (['New Save'] if self.page == 'save' and self.states else []) + entries + ['Back']

    def toggle(self, engine):
        self.open = not self.open
        self.page, self.selected, self.scroll = 'main', 0, 0
        engine.release_keys()
        engine.mouse_buttons = 0
        self.display.capture(False)
        print(f'overlay: {"paused" if self.open else "resumed"}', flush=True)

    def key(self, scan):
        rows = self.rows()
        if scan in (82, 81):
            self.selected = (self.selected + (1 if scan == 81 else -1)) % len(rows)
        elif scan == 41:
            if self.page == 'main':
                self.open = False
            else:
                self.page, self.selected = 'main', 0
        elif scan in (40, 44):
            self.activate()

    def activate(self):
        if self.page == 'main':
            if self.selected == 0:
                self.open = False
            elif self.selected == 1:
                self.display.toggle_fullscreen()
            elif self.selected in (2, 3):
                self.page = 'save' if self.selected == 2 else 'load'
                self.selected = 0
            else:
                self.display.closed = True
        elif self.selected == len(self.rows()) - 1:
            self.page, self.selected = 'main', 0
        elif self.states:
            entries = self.states.entries()
            path = None if self.page == 'save' and self.selected == 0 else entries[self.selected - (self.page == 'save')]
            self.states.pending = (self.page, path)
            self.open = False

    def click(self, x, y):
        width, height = self.display.dimensions or (640, 400)
        x = (x - self.display.rect.x) * width / max(1, self.display.rect.w)
        y = (y - self.display.rect.y) * height / max(1, self.display.rect.h)
        row = int((y - 64) // 22) + self.scroll
        if 12 <= x < width - 12 and y >= 64 and 0 <= row < len(self.rows()):
            self.selected = row
            self.activate()

    def render(self, pixels, width, height):
        pixels = bytearray(value // 4 for value in pixels)
        draw_text(pixels, width, height, 16, 12, f'EF2000 DEBUG - {self.page}', b'\xff\xff\xff')
        current = self.states.label(self.states.current) if self.states and self.states.current else 'None'
        draw_text(pixels, width, height, 16, 36, f'Current: {current}', b'\xaa\xaa\xaa', 1)
        count = max(1, (height - 92) // 22)
        self.scroll = min(self.scroll, self.selected)
        self.scroll = max(self.scroll, self.selected - count + 1)
        for index, row in enumerate(self.rows()[self.scroll:self.scroll + count], self.scroll):
            scale = 2 if self.page == 'main' else 1
            label = ('> ' if index == self.selected else '  ') + row
            draw_text(pixels, width, height, 16, 64 + (index - self.scroll) * 22,
                      label[:max(1, (width - 32) // (6 * scale))],
                      b'\xff\xdd\x55' if index == self.selected else b'\xdd\xdd\xdd', scale)
        draw_text(pixels, width, height, 16, height - 20, self.status or 'Arrows / Enter / Esc / F10', b'\xaa\xaa\xaa', 1)
        return bytes(pixels)
