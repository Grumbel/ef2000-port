#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Read simple EF2000 indexed .SPR collections (e.g. spr/pitfont.spr).

This preserves the original 16-bit sample values, not yet interpreted as
specific RGB colours, character codes, transparency, or font metrics.
"""

from dataclasses import dataclass
import struct


@dataclass(frozen=True)
class Sprite:
    width: int
    height: int
    pixels: tuple[int, ...]


def decode(data):
    if not data or data[0] == 0:
        raise ValueError('empty sprite collection')
    count = data[0]
    table_end = 1 + count * 2
    if table_end > len(data):
        raise ValueError('truncated sprite offset table')
    offsets = struct.unpack_from('<' + 'H' * count, data, 1)
    sprites = []
    for index, position in enumerate(offsets):
        end = offsets[index + 1] if index + 1 < count else len(data)
        if position < table_end or end < position + 2 or end > len(data):
            raise ValueError('sprite offset outside payload')
        width, height = struct.unpack_from('<BB', data, position)
        if not 0 < width <= 128 or not 0 < height <= 128:
            raise ValueError('unsupported sprite dimensions')
        if 2 + 2 * width * height != end - position:
            raise ValueError('sprite pixel-record length mismatch')
        pixels = struct.unpack_from('<' + 'H' * (width * height), data, position + 2)
        sprites.append(Sprite(width, height, pixels))
    return sprites
