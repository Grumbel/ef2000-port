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

    def save_state(self):
        return dict(vars(self))

    def restore_state(self, state):
        self.__dict__.update(state)
        self.data = deque(self.data)
        self.resources = {int(key): value for key, value in self.resources.items()}

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


class SoundBlaster16(SB16Probe):
    """DMA-backed SB16 output; completed blocks retain original guest PCM."""

    def __init__(self):
        super().__init__()
        try:
            from .isa_audio import AudioPIC, DMA8237
        except ImportError:
            from isa_audio import AudioPIC, DMA8237
        self.pic = AudioPIC()
        self.dma = DMA8237()
        self.sample_rate = 22050
        self.command = None
        self.arguments = []
        self.needed = 0
        self.active = None
        self.completed = []
        self.speaker = False
        self.mixer_values = {register: 0xf8 for register in range(0x30, 0x3c)}
        self.generation = 0

    def save_state(self):
        import time
        state = super().save_state()
        state['pic'] = dict(vars(self.pic))
        state['dma'] = dict(vars(self.dma))
        if self.active is not None:
            state['active'] = dict(self.active)
            state['active']['deadline'] -= time.monotonic()
        return state

    def restore_state(self, state):
        import time
        state = dict(state)
        self.pic.__dict__.update(state.pop('pic'))
        self.dma.__dict__.update(state.pop('dma'))
        super().restore_state(state)
        self.mixer_values = {int(key): value for key, value in self.mixer_values.items()}
        if self.active is not None:
            self.active = dict(self.active)
            self.active['deadline'] += time.monotonic()

    def handles(self, port):
        return 0x220 <= port <= 0x22f or port in (0x20, 0x21, 0xa0, 0xa1, 0x3da) or self.dma.handles(port)

    def read(self, port, size=1):
        self.check_width(size)
        if port in (0x20, 0x21, 0xa0, 0xa1):
            return self.pic.read(port)
        if self.dma.handles(port):
            return self.dma.read(port)
        if port == 0x3da:
            import time
            return 8 if time.monotonic_ns() % 16666667 >= 15000000 else 0
        if port == 0x22f:
            self.resources[0x82] &= ~2
            return 0xff
        if port == 0x22e:
            self.resources[0x82] &= ~1
        if port == 0x225 and self.mixer_index in self.mixer_values:
            return self.mixer_values[self.mixer_index]
        return super().read(port, size)

    def write(self, port, value, size=1):
        self.check_width(size)
        if port in (0x20, 0x21, 0xa0, 0xa1):
            self.pic.write(port, value)
            return
        if self.dma.handles(port):
            self.dma.write(port, value)
            return
        if port == 0x226:
            if value & 1:
                self.generation += 1
                self.active = None
                self.command = None
                self.arguments = []
                self.resources[0x82] = 0x20
                self.pic.pending[0] &= ~0x80
            return super().write(port, value, size)
        if port == 0x225:
            if self.mixer_index in (0x04, 0x22, 0x26, 0x28, *range(0x30, 0x48)):
                self.mixer_values[self.mixer_index] = value
                return
            raise ValueError(f'unsupported SB16 mixer write {self.mixer_index:#x}')
        if port != 0x22c:
            return super().write(port, value, size)
        if not self.initialized:
            raise ValueError('SB16 DSP command before reset')
        if self.command is not None:
            self.arguments.append(value)
            if len(self.arguments) == self.needed:
                self.finish_command()
            return
        if value == 0xe1:
            self.data.extend((4, 5))
        elif value in (0x41, 0x42):
            self.command, self.needed, self.arguments = value, 2, []
        elif value in (0xb0, 0xb2, 0xb4, 0xb6, 0xc0, 0xc2, 0xc4, 0xc6):
            self.command, self.needed, self.arguments = value, 3, []
        elif value in (0xd0, 0xd5):
            if self.active:
                self.active['paused'] = True
        elif value in (0xd4, 0xd6):
            if self.active:
                import time
                self.active['paused'] = False
                self.active['deadline'] = time.monotonic() + self.active['duration']
        elif value in (0xda, 0xd9):
            if self.active:
                self.active['auto'] = False
        elif value in (0xd1, 0xd3):
            self.speaker = value == 0xd1
        elif value == 0xd8:
            self.data.append(0xff if self.speaker else 0)
        else:
            raise ValueError(f'unsupported SB16 DSP command {value:#x}')

    def finish_command(self):
        import time
        command, args = self.command, self.arguments
        self.command, self.arguments = None, []
        if command in (0x41, 0x42):
            rate = args[0] * 256 + args[1]
            if not rate:
                raise ValueError('zero SB16 sample rate')
            self.sample_rate = rate
            return
        mode, low, high = args
        if mode & ~0x30:
            raise ValueError(f'unsupported SB16 sample mode {mode:#x}')
        bits = 16 if command & 0xf0 == 0xb0 else 8
        channels = 2 if mode & 0x20 else 1
        units = (high * 256 + low) + 1
        duration = units / (self.sample_rate * channels)
        self.active = {'bits': bits, 'channels': channels, 'signed': bool(mode & 0x10),
                       'rate': self.sample_rate, 'units': units, 'auto': bool(command & 4),
                       'duration': duration, 'deadline': time.monotonic() + duration, 'paused': False}

    def advance(self, now, read_memory):
        transfer = self.active
        if not transfer or transfer['paused'] or now < transfer['deadline']:
            return
        # DMA keeps running while an interrupt is pending. Coalesce IRQs as
        # hardware does, but retain elapsed PCM blocks rather than repeatedly
        # interrupting the guest until it catches up with an old deadline.
        channel = 5 if transfer['bits'] == 16 else 1
        while now >= transfer['deadline']:
            pcm = self.dma.transfer(channel, transfer['units'], read_memory)
            self.completed.append({key: transfer[key] for key in ('bits', 'channels', 'signed', 'rate')} |
                                  {'pcm': pcm, 'generation': self.generation,
                                   'gains': tuple(((self.mixer_values[0x30 + i] >> 3) / 31) *
                                                  ((self.mixer_values[0x32 + i] >> 3) / 31)
                                                  for i in range(2))})
            if not self.resources[0x82] & 3:
                self.resources[0x82] |= 2 if transfer['bits'] == 16 else 1
                self.pic.request(7)
            if transfer['auto']:
                transfer['deadline'] += transfer['duration']
            else:
                self.active = None
                break
