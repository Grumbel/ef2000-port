# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Strict SB16 discovery model, not an audio or DMA implementation."""

from collections import deque


class SB16Probe:
    """Measured DOSBox-X profile: DSP 4.5, base 220h, IRQ7, DMA1/5."""

    def __init__(self):
        self.reset_asserted = False
        self.initialized = False
        self.data = deque()
        self.mixer_index = 0
        self.resources = {0x80: 0x04, 0x81: 0x22, 0x82: 0x20}

    @staticmethod
    def check_width(size):
        if size != 1:
            raise ValueError(f'unsupported SB16 probe port width {size}')

    def read(self, port, size=1):
        self.check_width(size)
        if port == 0x226:  # Write-only reset register reads as floating bus.
            return 0xff
        if port == 0x22c:
            return 0x7f  # Bit 7 clear: DSP write buffer available.
        if port == 0x22e:
            return 0xff if self.data else 0x7f
        if port == 0x22a:
            return self.data.popleft() if self.data else 0xff
        if port == 0x225 and self.mixer_index in self.resources:
            return self.resources[self.mixer_index]
        raise ValueError(f'unsupported SB16 probe IN port {port:#x}, mixer {self.mixer_index:#x}')

    def write(self, port, value, size=1):
        self.check_width(size)
        if not 0 <= value <= 255:
            raise ValueError('SB16 probe byte outside port width')
        if port == 0x226:
            if value & 1:
                self.reset_asserted = True
                self.initialized = False
                self.data.clear()
            elif self.reset_asserted:
                self.reset_asserted = False
                self.initialized = True
                self.data.append(0xaa)
            return
        if port == 0x224:
            self.mixer_index = value
            return
        if port == 0x22c:
            if not self.initialized:
                raise ValueError('SB16 DSP command before reset')
            if value == 0xe1:
                self.data.extend((4, 5))
                return
            raise ValueError(f'unsupported SB16 DSP command {value:#x}; no audio/DMA implementation')
        raise ValueError(f'unsupported SB16 probe OUT port {port:#x}, mixer {self.mixer_index:#x}')
