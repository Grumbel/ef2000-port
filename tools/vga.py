#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Render the original mode-13h framebuffer and six-bit VGA DAC palette."""

import struct
import zlib

WIDTH, HEIGHT = 320, 200


def rgb(memory):
    if len(memory) != 0x100000:
        raise ValueError('expected a complete 1 MiB DOS memory snapshot')
    return indexed_rgb(memory[0xa0000:0xa0000 + WIDTH * HEIGHT], memory[0xf1000:0xf1300])


def indexed_rgb(indices, colors):
    palette = [bytes((min(value, 63) * 255 + 31) // 63 for value in colors[i * 3:i * 3 + 3])
               for i in range(256)]
    return b''.join(palette[index] for index in indices)


def save(memory, path):
    save_indexed(memory[0xa0000:0xa0000 + WIDTH * HEIGHT], memory[0xf1000:0xf1300], WIDTH, HEIGHT, path)


def save_indexed(indices, palette, width, height, path):
    pixels = indexed_rgb(indices, palette)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
