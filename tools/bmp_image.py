#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Decode palette-preserving, uncompressed 8-bit Windows BMP archive images."""

import struct

try:
    from .ilbm_image import Image
except ImportError:
    from ilbm_image import Image


def is_bmp(data):
    """Recognize a likely Windows BMP before attempting a full decode."""
    return (len(data) >= 54 and data[:2] == b'BM' and
            struct.unpack_from('<I', data, 14)[0] >= 40)


def decode(data):
    """Return an 8-bit indexed image with exact palette and pixel indices.

    Supports BITMAPINFOHEADER and larger compatible DIB headers, including
    four-byte scanline padding and bottom-up/top-down pixel order. Rejects
    RLE and true-colour variants until their conversion is specified.
    """
    if not is_bmp(data):
        raise ValueError('expected Windows BMP with DIB header')
    pixel_offset = struct.unpack_from('<I', data, 10)[0]
    dib_size, width, signed_height, planes, bpp, compression = struct.unpack_from(
        '<IiiHHI', data, 14)
    if dib_size > len(data) - 14:
        raise ValueError('truncated BMP DIB header')
    if not 0 < width <= 8192 or not 0 < abs(signed_height) <= 8192:
        raise ValueError('unsupported BMP dimensions')
    if planes != 1 or bpp != 8 or compression != 0:
        raise ValueError('unsupported BMP planes, depth or compression')
    used_colors = struct.unpack_from('<I', data, 46)[0]
    colors = used_colors or 256
    if not 1 <= colors <= 256:
        raise ValueError('unsupported BMP palette size')
    palette_start = 14 + dib_size
    if palette_start + colors * 4 > pixel_offset:
        raise ValueError('truncated BMP palette')
    stride = (width + 3) & ~3
    height = abs(signed_height)
    if pixel_offset + stride * height > len(data):
        raise ValueError('truncated BMP pixels')
    palette = bytearray(768)
    for index in range(colors):
        blue, green, red, _ = struct.unpack_from('<4B', data, palette_start + 4 * index)
        palette[3 * index:3 * index + 3] = bytes((red, green, blue))
    pixels = bytearray(width * height)
    for y in range(height):
        stored_y = height - 1 - y if signed_height > 0 else y
        start = pixel_offset + stored_y * stride
        pixels[y * width:(y + 1) * width] = data[start:start + width]
    return Image(width, height, bytes(pixels), bytes(palette))
