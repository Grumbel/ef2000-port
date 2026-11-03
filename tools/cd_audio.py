# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Read-only, single-BINARY CUE disc and MSCDEX audio request handling."""
import re
import struct
import time
from pathlib import Path


class CDAudio:
    def __init__(self, cue):
        cue = Path(cue)
        source = cue.read_text()
        files = re.findall(r'^\s*FILE\s+"([^"]+)"\s+BINARY\s*$', source, re.M | re.I)
        if len(files) != 1:
            raise ValueError('CD audio requires one BINARY file in the CUE')
        self.path = cue.parent / files[0]
        self.sectors, remainder = divmod(self.path.stat().st_size, 2352)
        if remainder:
            raise ValueError('CD image size is not a whole number of raw sectors')
        self.tracks = []
        for number, kind, body in re.findall(r'TRACK\s+(\d+)\s+(\S+)(.*?)(?=TRACK|\Z)', source, re.S | re.I):
            index = re.search(r'INDEX\s+0?1\s+(\d+):(\d+):(\d+)', body, re.I)
            if not index or kind.upper() not in ('AUDIO', 'MODE1/2352'):
                raise ValueError('unsupported CUE track')
            m, s, f = map(int, index.groups())
            start = (m * 60 + s) * 75 + f
            if s >= 60 or f >= 75 or start >= self.sectors:
                raise ValueError('invalid CUE track position')
            self.tracks.append((int(number), kind.upper(), start))
        if not self.tracks or [t[0] for t in self.tracks] != list(range(1, len(self.tracks) + 1)):
            raise ValueError('CUE tracks must be consecutive starting at 1')
        if any(a[2] >= b[2] for a, b in zip(self.tracks, self.tracks[1:])):
            raise ValueError('CUE track positions must increase')
        self.file = self.path.open('rb')
        self.start = self.end = self.cursor = 0
        self.started = None
        self.generation = 0
        self.events = []

    @staticmethod
    def msf(sector):
        sector += 150
        return bytes((sector % 75, sector // 75 % 60, sector // 4500, 0))

    def position(self):
        if self.started is None:
            return self.cursor
        return min(self.end, self.start + int((time.monotonic() - self.started) * 75))

    def playing(self):
        return self.started is not None and self.position() < self.end

    def pcm(self):
        end = self.position()
        if end <= self.cursor:
            return None
        self.file.seek(self.cursor * 2352)
        pcm = self.file.read((end - self.cursor) * 2352)
        self.cursor = end
        return dict(pcm=pcm, bits=16, channels=2, signed=True, rate=44100,
                    generation=self.generation)

    def interrupt(self, values, memory):
        memory = bytearray(memory)
        ax = values[2] & 0xffff
        if ax == 0x1500:
            values[3], values[4] = 1, 3  # One drive, D:.
        elif ax == 0x150c:
            values[3] = 0x020a
        elif ax == 0x150b:
            values[2], values[3] = int(values[4] == 3), 0xadad
        elif ax == 0x1505 and values[4] & 0xffff == 3:
            address = (values[9] << 4) + (values[3] & 0xffff)
            if address + 2048 > len(memory):
                raise ValueError('CD volume descriptor outside conventional memory')
            self.file.seek((16 + (values[5] & 0xffff)) * 2352 + 16)
            descriptor = self.file.read(2048)
            memory[address:address+2048] = descriptor
            values[2] = 1 if descriptor[1:6] == b'CD001' else 0xff
        elif ax == 0x1510 and values[4] & 0xffff == 3:
            address = (values[9] << 4) + (values[3] & 0xffff)
            def check(offset, size):
                if offset < 0 or offset + size > len(memory):
                    raise ValueError('CD request outside conventional memory')
            check(address, 22)
            command = memory[address + 2]
            status = 0x100
            event = dict(command=command)
            if command in (3, 12):
                offset, segment = struct.unpack_from('<HH', memory, address + 14)
                data = segment * 16 + offset
                size = struct.unpack_from('<H', memory, address + 18)[0]
                check(data, size)
                sub = memory[data] if size else -1
                event['subcommand'] = sub
                result = None
                if command == 3:
                    if sub == 10:
                        result = bytes((10, 1, len(self.tracks))) + self.msf(self.sectors)
                    elif sub == 11 and size >= 2 and 1 <= memory[data+1] <= len(self.tracks):
                        number, kind, start = self.tracks[memory[data+1]-1]
                        result = bytes((11, number)) + self.msf(start) + bytes((0x40 if kind != 'AUDIO' else 0,))
                    elif sub == 6:
                        result = b'\x06' + struct.pack('<I', 0x290)  # audio, raw reads, Red Book addressing
                    elif sub == 9:
                        result = b'\x09\x01'  # media unchanged
                    elif sub == 12:
                        result = bytes((12,)) + bytes(10)
                    if result is None or len(result) > size:
                        status = 0x8103
                    else:
                        memory[data:data+len(result)] = result
                elif sub == 2:  # Reset drive.
                    self.started = None
                else:
                    status = 0x8103
            elif command == 0x84:
                mode = memory[address+13]
                start, length = struct.unpack_from('<II', memory, address+14)
                if mode == 1:
                    start = (start >> 16 & 255) * 4500 + (start >> 8 & 255) * 75 + (start & 255) - 150
                end = start + length
                track = next((t for t in reversed(self.tracks) if t[2] <= start), None)
                if mode not in (0, 1) or not track or track[1] != 'AUDIO' or not start < end <= self.sectors:
                    status = 0x8108
                else:
                    self.start = self.cursor = start
                    self.end = end
                    self.started = time.monotonic()
                    self.generation += 1
                    event.update(start=start, end=end)
            elif command == 0x85:
                self.cursor = self.position()
                self.started = None
            else:
                status = 0x8103
            if self.playing():
                status |= 0x200
            struct.pack_into('<H', memory, address+3, status)
            event['status'] = status
            self.events.append(event)
        else:
            values[2] = 15
            values[-2] |= 1
            return values, bytes(memory)
        values[-2] &= ~1
        return values, bytes(memory)

    def close(self):
        self.file.close()
