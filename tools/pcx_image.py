#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Read 256-colour, 8-bit PCX UI artwork into the common indexed image type."""

import struct

try:
    from .ilbm_image import Image
except ImportError:
    from ilbm_image import Image


def is_pcx(data):
    return (len(data) >= 128 + 769 and data[0] == 10 and data[2:4] == b'\x01\x08'
            and data[65] == 1 and data[-769] == 12)


def decode(data):
    if not is_pcx(data):
        raise ValueError('expected 256-colour, 8-bit RLE PCX with trailing palette')
    left, top, right, bottom = struct.unpack_from('<4H', data, 4)
    width = right - left + 1
    height = bottom - top + 1
    stride = struct.unpack_from('<H', data, 66)[0]
    if not 0 < width <= 4096 or not 0 < height <= 4096 or not width <= stride <= 4096:
        raise ValueError('unsupported PCX geometry or bytes-per-line')
    source = memoryview(data)[128:-769]
    result = bytearray()
    position = 0
    expected = stride * height
    while len(result) < expected:
        if position >= len(source):
            raise ValueError('truncated PCX image')
        value = source[position]
        position += 1
        if (value & 0xc0) == 0xc0:
            count = value & 0x3f
            if position >= len(source) or count == 0:
                raise ValueError('invalid PCX run')
            value = source[position]
            position += 1
        else:
            count = 1
        if len(result) + count > expected:
            raise ValueError('PCX run exceeds image extent')
        result.extend(bytes([value]) * count)
    if position != len(source):
        raise ValueError('unexpected PCX image trailer')
    pixels = b''.join(result[y * stride:y * stride + width] for y in range(height))
    return Image(width, height, pixels, data[-768:])
