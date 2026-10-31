# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""SDL physical keys to PC/AT set-1 scan codes and host input mailbox."""

from collections import deque
import struct

INPUT_BASE = 0xf1400
JOYSTICK_BASE = 0xf1600
INPUT_MAGIC = 0x31504e49
QUEUE_SIZE = 256

SCANS = dict(zip(range(4, 30), (
    0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
    0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
    0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c)))
SCANS.update(zip(range(30, 40), range(2, 12)))
SCANS.update(zip(range(40, 58), (
    0x1c, 1, 0xe, 0xf, 0x39, 0xc, 0xd, 0x1a, 0x1b, 0x2b,
    0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, 0x3a)))
SCANS.update(zip(range(58, 68), range(0x3b, 0x45)))
SCANS.update({68: 0x57, 69: 0x58, 71: 0x46, 83: 0x45,
              85: 0x37, 86: 0x4a, 87: 0x4e, 100: 0x56,
              224: 0x1d, 225: 0x2a, 226: 0x38, 229: 0x36})
SCANS.update(zip(range(89, 100), (
    0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, 0x48, 0x49, 0x52, 0x53)))
EXTENDED = dict(zip(range(73, 83), (
    0x52, 0x47, 0x49, 0x53, 0x4f, 0x51, 0x4d, 0x4b, 0x50, 0x48)))
EXTENDED.update({84: 0x35, 88: 0x1c, 228: 0x1d, 230: 0x38})


def flight_control_settings(lines, joystick):
    result = []
    for line in lines:
        name, separator, value = line.partition(b'=')
        if name == b'CONTROL':
            value = b'1' if joystick else b'0'
        elif name == b'JOY_FACTOR' and joystick and value.strip() == b'0':
            # The executable defaults to 100 percent. Digital-sound startup
            # reapplies this scale to calibration; zero disables both axes.
            value = b'100'
        result.append(name + separator + value)
    return result


def scan_bytes(scan, down):
    if scan == 70:  # Print Screen includes the synthetic shift prefix.
        return bytes.fromhex('e02ae037' if down else 'e0b7e0aa')
    if scan == 72:  # Pause has no break sequence.
        return bytes.fromhex('e11d45e19dc5') if down else b''
    code = EXTENDED.get(scan, SCANS.get(scan))
    if code is None:
        return b''
    return (b'\xe0' if scan in EXTENDED else b'') + bytes((code | (0 if down else 0x80),))


class HostInput:
    def __init__(self):
        self.scan_queue = deque()
        self.pressed_keys = set()
        self.keys = []
        self.shift_flags = 0
        self.joystick_axes = []
        self.joystick_buttons = 0

    def key_event(self, scan, key, down, repeat=False):
        encoded = scan_bytes(scan, down)
        if not encoded:
            if not down:
                self.pressed_keys.discard(scan)
            return
        if down:
            self.pressed_keys.add(scan)
        elif scan not in self.pressed_keys:
            return
        else:
            self.pressed_keys.remove(scan)
        self.scan_queue.extend(encoded)
        if down and not repeat:
            self.shift_flags ^= {57: 0x40, 83: 0x20, 71: 0x10}.get(scan, 0)
        self.shift_flags = (self.shift_flags & 0xf0) | (
            (1 if 229 in self.pressed_keys else 0) |
            (2 if 225 in self.pressed_keys else 0) |
            (4 if self.pressed_keys & {224, 228} else 0) |
            (8 if self.pressed_keys & {226, 230} else 0))
        if down and scan not in (224, 225, 226, 228, 229, 230, 57, 71, 83) and len(self.keys) < 64:
            code = EXTENDED.get(scan, SCANS.get(scan, 0))
            char = key if 0 <= key < 256 else 0
            if 97 <= char <= 122:
                if self.shift_flags & 4:
                    char -= 96
                elif bool(self.shift_flags & 3) != bool(self.shift_flags & 0x40):
                    char -= 32
            if self.shift_flags & 8:
                char = 0
            self.keys.append((code << 8) | char)

    def release_keys(self):
        for scan in sorted(self.pressed_keys):
            self.key_event(scan, 0, False)

    def sync_input(self, memory):
        memory = bytearray(memory)
        if struct.unpack_from('<I', memory, INPUT_BASE)[0] == INPUT_MAGIC:
            head, tail = struct.unpack_from('<HH', memory, INPUT_BASE + 4)
            head, tail = head % QUEUE_SIZE, tail % QUEUE_SIZE
        else:
            head = tail = 0
        while self.scan_queue and (tail + 1) % QUEUE_SIZE != head:
            memory[INPUT_BASE + 8 + tail] = self.scan_queue.popleft()
            tail = (tail + 1) % QUEUE_SIZE
        struct.pack_into('<IHH', memory, INPUT_BASE, INPUT_MAGIC, head, tail)
        axes = self.joystick_axes[:4]
        # Deterministic game-port discharge steps. The original game measures
        # these by polling; its own calibration converts them to stick travel.
        counts = [1 + (max(-32768, min(32767, v)) + 32768) * 126 // 65535 for v in axes]
        struct.pack_into('<BB4H', memory, JOYSTICK_BASE, (1 << len(axes)) - 1,
                         self.joystick_buttons & 15, *(counts + [0] * (4 - len(counts))))
        return memory
