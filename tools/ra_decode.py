#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Bounded decoder for the DID archive's RA LZ/Huffman member compression.

Format confirmed against the original EF2000 archive and the public tfx3view
reverse engineering (whose C++ decoder credits Benjamin Haisch, 2006).
"""

import struct


class _Bits:
    def __init__(self, data):
        self.data = data
        # The first flag byte is prefetched, with its top bit consumed.
        self.position = 13
        self.current = (data[12] << 1) & 255
        self.remaining = 7

    def byte(self):
        if self.position >= len(self.data):
            raise ValueError('truncated RA stream')
        result = self.data[self.position]
        self.position += 1
        return result

    def bit(self):
        if self.remaining == 0:
            self.current = self.byte()
            self.remaining = 8
        result = (self.current >> 7) & 1
        self.current = (self.current << 1) & 255
        self.remaining -= 1
        return result

    def bits(self, count):
        if count > 24:
            raise ValueError('oversized RA bit read')
        value = 0
        for _ in range(count):
            value = (value << 1) | self.bit()
        return value

    def table(self, capacity):
        count = 2 * self.byte()
        if not count or count > capacity:
            raise ValueError('invalid RA Huffman table size')
        length_a = 1 + self.bits(3)
        length_b = 1 + self.bits(3)
        branch = [0] * capacity
        value = [0] * capacity
        for index in range(count):
            if self.bit():
                value[index] = self.bits(length_b)
            else:
                branch[index] = self.bits(length_a)
        return branch, value

    def symbol(self, table):
        branches, values = table
        branch = 0
        for _ in range(128):
            index = (branch << 1) | self.bit()
            if index >= len(branches):
                raise ValueError('invalid RA Huffman branch')
            branch = branches[index]
            if branch == 0:
                return values[index]
        raise ValueError('cyclic RA Huffman table')


def decompress(data, *, max_output=128 * 1024 * 1024, prefix=None):
    if len(data) < 15 or data[:2] != b'RA':
        raise ValueError('invalid RA header')
    if struct.unpack_from('<H', data, 2)[0] != 512:
        raise ValueError('unsupported RA version')
    expected = struct.unpack_from('<I', data, 4)[0]
    if expected > max_output:
        raise ValueError('RA output size exceeds limit')
    if prefix is not None and prefix <= 0:
        raise ValueError('RA prefix length must be positive')
    bits = _Bits(data)
    result = bytearray()
    blocks = 0
    while True:
        blocks += 1
        if blocks > 65536:
            raise ValueError('too many RA blocks')
        size = bits.byte() | (bits.byte() << 8)
        lengths = bits.table(512)
        counts = bits.table(512)
        offsets = bits.table(32)
        for _ in range(size):
            raw_count = bits.symbol(counts)
            if bits.position + raw_count > len(data) or len(result) + raw_count > expected:
                raise ValueError('RA literal copy exceeds input or output')
            result.extend(data[bits.position:bits.position + raw_count])
            bits.position += raw_count
            if prefix is not None and len(result) >= prefix:
                return bytes(result[:prefix])
            length = bits.symbol(lengths)
            if length == 255:
                length = ((bits.bits(2) << 8) | bits.byte()) + 255
            if length:
                code = bits.symbol(offsets)
                if code >= 2:
                    code = (1 << (code - 1)) | bits.bits(code - 1)
                back = code + 1
                if back > len(result) or len(result) + length > expected:
                    raise ValueError('RA back-reference exceeds output range')
                for _ in range(length):
                    result.append(result[-back])
                    if prefix is not None and len(result) >= prefix:
                        return bytes(result[:prefix])
        if not bits.bit():
            break
    if len(result) != expected:
        raise ValueError(f'RA output length mismatch: {len(result)} != {expected}')
    return bytes(result[:prefix] if prefix is not None else result)
