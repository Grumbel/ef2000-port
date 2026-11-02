#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Decode EF2000's standard IFF ILBM/PBM menu artwork into indexed PNG.

No Pillow, ImageMagick or proprietary image data is needed at build time.
"""

from dataclasses import dataclass
import struct
import zlib


@dataclass(frozen=True)
class Image:
    width: int
    height: int
    indexed: bytes
    palette: bytes
    alpha: bytes | None = None


def chunks(data):
    if len(data) < 12 or data[:4] != b'FORM' or data[8:12] not in (b'ILBM', b'PBM '):
        raise ValueError('expected IFF FORM ILBM or PBM')
    end = 8 + struct.unpack_from('>I', data, 4)[0]
    if end < 12 or end > len(data):
        raise ValueError('truncated IFF FORM')
    offset = 12
    sections = {}
    while offset < end:
        if offset + 8 > end:
            raise ValueError('truncated IFF chunk header')
        tag, size = struct.unpack_from('>4sI', data, offset)
        offset += 8
        if size > end - offset:
            raise ValueError('truncated IFF chunk body')
        sections[tag] = data[offset:offset + size]
        offset += size + (size & 1)
    if offset != end:
        raise ValueError('invalid IFF chunk padding')
    return data[8:12], sections


def byterun_row(data, position, count):
    output = bytearray()
    while len(output) < count:
        if position >= len(data):
            raise ValueError('truncated ByteRun1 row')
        op = data[position]
        position += 1
        if op <= 127:
            length = op + 1
            if position + length > len(data):
                raise ValueError('truncated ByteRun1 literal')
            output.extend(data[position:position + length])
            position += length
        elif op >= 129:
            if position >= len(data):
                raise ValueError('truncated ByteRun1 repeat')
            output.extend(bytes([data[position]]) * (257 - op))
            position += 1
        # 128 is a NOP.
        if len(output) > count:
            raise ValueError('ByteRun1 crossed scanline boundary')
    return bytes(output), position


def decode(data):
    kind, sections = chunks(data)
    if b'BMHD' not in sections or b'BODY' not in sections:
        raise ValueError('missing BMHD or BODY chunk')
    bmhd = sections[b'BMHD']
    if len(bmhd) < 20:
        raise ValueError('short BMHD chunk')
    width, height, _, _, planes, masking, compression = struct.unpack_from('>HHhhBBB', bmhd)
    transparent = struct.unpack_from('>H', bmhd, 12)[0]
    if not 0 < width <= 4096 or not 0 < height <= 4096:
        raise ValueError('unsupported bitmap dimensions')
    if not 1 <= planes <= 8 or masking not in (0, 1, 2) or compression not in (0, 1):
        raise ValueError('unsupported bitmap planes, mask or compression')
    palette = sections.get(b'CMAP', b'')
    if not palette or len(palette) % 3 or len(palette) > 768:
        raise ValueError('missing or malformed 8-bit CMAP')
    palette += bytes(768 - len(palette))
    body = sections[b'BODY']
    pixels = bytearray(width * height)
    alpha = bytearray(width * height) if masking == 1 else None
    position = 0
    stride = ((width + 15) // 16) * 2 if kind == b'ILBM' else width + (width & 1)
    planes_per_row = planes + (masking == 1) if kind == b'ILBM' else 1
    if kind == b'PBM ' and (masking == 1 or planes != 8):
        raise ValueError('unsupported PBM mask or bit depth')
    for y in range(height):
        for plane in range(planes_per_row):
            if compression == 1:
                row, position = byterun_row(body, position, stride)
            else:
                if position + stride > len(body):
                    raise ValueError('truncated image row')
                row = body[position:position + stride]
                position += stride
            start = y * width
            if kind == b'PBM ':
                pixels[start:start + width] = row[:width]
            elif plane == planes:
                for x in range(width):
                    alpha[start + x] = 255 if row[x // 8] & (128 >> (x % 8)) else 0
            else:
                bit = 1 << plane
                for x in range(width):
                    if row[x // 8] & (128 >> (x % 8)):
                        pixels[start + x] |= bit
    # A few shipped EF2000 BODY chunks include one internal zero pad even
    # though their IFF chunk length already accounts for the usual alignment.
    if position + 1 == len(body) and body[position] == 0:
        position += 1
    if position != len(body):
        raise ValueError('unexpected trailing bitmap data')
    if masking == 2:
        if transparent >= (1 << planes):
            raise ValueError('invalid transparent colour index')
        alpha = bytes(0 if p == transparent else 255 for p in pixels)
    return Image(width, height, bytes(pixels), palette,
                 bytes(alpha) if alpha is not None else None)


def encode_png(image):
    def chunk(tag, body):
        return (struct.pack('>I', len(body)) + tag + body +
                struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff))

    width = image.width
    height = image.height
    if image.alpha is None:
        # Preserve the game's exact palette indices; no RGB quantization.
        scanlines = b''.join(b'\0' + image.indexed[y * width:(y + 1) * width]
                             for y in range(height))
        payload = chunk(b'PLTE', image.palette)
        colour_type = 3
    else:
        scanlines = bytearray()
        for y in range(height):
            scanlines.append(0)
            for x in range(width):
                n = y * width + x
                rgb = image.palette[3 * image.indexed[n]:3 * image.indexed[n] + 3]
                scanlines.extend(rgb)
                scanlines.append(image.alpha[n])
        payload = b''
        colour_type = 6
    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, colour_type, 0, 0, 0)) +
            payload + chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))
