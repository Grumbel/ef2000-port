# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""ISA PIC and DMA state used by the original Sound Blaster driver."""


class AudioPIC:
    def __init__(self):
        self.masks = [0xf8, 0xff]
        self.pending = [0, 0]
        self.servicing = [0, 0]
        self.read_isr = [False, False]

    def read(self, port):
        chip = int(port >= 0xa0)
        if port & 1:
            return self.masks[chip]
        return (self.servicing if self.read_isr[chip] else self.pending)[chip]

    def write(self, port, value):
        chip = int(port >= 0xa0)
        if port & 1:
            self.masks[chip] = value
        elif value in (0x0a, 0x0b):
            self.read_isr[chip] = value == 0x0b
        elif value == 0x20:
            bits = self.servicing[chip]
            self.servicing[chip] &= ~(bits & -bits)
        else:
            raise ValueError(f'unsupported audio PIC command {value:#x}')

    def request(self, irq):
        self.pending[irq // 8] |= 1 << (irq % 8)

    def next_irq(self):
        for irq in range(8):
            if self.pending[0] & ~self.masks[0] & (1 << irq) and not self.servicing[0]:
                return irq
        return None

    def begin(self, irq):
        bit = 1 << irq
        self.pending[0] &= ~bit
        self.servicing[0] |= bit


class DMA8237:
    PAGE_PORTS = {0x87: 0, 0x83: 1, 0x81: 2, 0x82: 3, 0x8b: 5, 0x89: 6, 0x8a: 7}

    def __init__(self):
        self.address = [0] * 8
        self.count = [0xffff] * 8
        self.base_address = [0] * 8
        self.base_count = [0xffff] * 8
        self.page = [0] * 8
        self.masked = [True] * 8
        self.mode = [0] * 8
        self.high = [False, False]

    @classmethod
    def handles(cls, port):
        return port in cls.PAGE_PORTS or port in (
            *range(8), 0x0a, 0x0b, 0x0c, *range(0xc0, 0xd0, 2), 0xd4, 0xd6, 0xd8)

    def write(self, port, value):
        if port in self.PAGE_PORTS:
            self.page[self.PAGE_PORTS[port]] = value
            return
        chip = int(port >= 0xc0)
        local = (port - 0xc0) // 2 if chip else port
        if local == 0xc:
            self.high[chip] = False
        elif local == 0xa:
            self.masked[chip * 4 + (value & 3)] = bool(value & 4)
        elif local == 0xb:
            self.mode[chip * 4 + (value & 3)] = value
        elif 0 <= local < 8:
            channel = chip * 4 + local // 2
            target = self.count if local & 1 else self.address
            base = self.base_count if local & 1 else self.base_address
            shift = 8 if self.high[chip] else 0
            target[channel] = (target[channel] & (0xff if shift else 0xff00)) | (value << shift)
            base[channel] = target[channel]
            self.high[chip] = not self.high[chip]
        else:
            raise ValueError(f'unsupported DMA write port {port:#x}')

    def read(self, port):
        if port in self.PAGE_PORTS:
            return self.page[self.PAGE_PORTS[port]]
        chip = int(port >= 0xc0)
        local = (port - 0xc0) // 2 if chip else port
        if not 0 <= local < 8:
            raise ValueError(f'unsupported DMA read port {port:#x}')
        channel = chip * 4 + local // 2
        value = (self.count if local & 1 else self.address)[channel]
        result = (value >> (8 if self.high[chip] else 0)) & 255
        self.high[chip] = not self.high[chip]
        return result

    def transfer(self, channel, units, read_memory):
        if self.masked[channel]:
            raise ValueError(f'audio DMA channel {channel} is masked')
        if self.mode[channel] & 0x0c != 8 or self.mode[channel] & 0x20:
            raise ValueError(f'unsupported audio DMA mode {self.mode[channel]:#x}')
        scale = 2 if channel >= 4 else 1
        output = bytearray()
        while units:
            take = min(units, self.count[channel] + 1, 0x10000 - self.address[channel])
            address = ((self.page[channel] & (0xfe if scale == 2 else 0xff)) << 16) | (self.address[channel] * scale)
            output.extend(read_memory(address, take * scale))
            self.address[channel] = (self.address[channel] + take) & 0xffff
            self.count[channel] -= take
            units -= take
            if self.count[channel] < 0:
                self.count[channel] = 0xffff
                if self.mode[channel] & 0x10:
                    self.address[channel] = self.base_address[channel]
                    self.count[channel] = self.base_count[channel]
                else:
                    self.masked[channel] = True
                    if units:
                        raise ValueError('audio transfer exceeds programmed DMA count')
        return bytes(output)
