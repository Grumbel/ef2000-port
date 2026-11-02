#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Recover menu glyph bitmaps from supplied, already-rasterized EF2000 art.

The original 640x400 option screens contain clean, non-antialiased white text.
Each non-space glyph occupies contiguous lit columns; letters have two blank
columns between them. These measured line rectangles are for the German
TACTCOM DID1ST.DAT revision. No original image or font data lives in Git.
"""

from dataclasses import dataclass
from collections import Counter, defaultdict
import json
from pathlib import Path
import string

try:
    from .did_archive import DIDArchive
    from .ilbm_image import Image, decode, encode_bmp, encode_png
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import Image, decode, encode_bmp, encode_png


CELL_WIDTH = 8
CELL_HEIGHT = 12
ASCII_START = 32
ASCII_END = 126
WHITE_THRESHOLD = 185


@dataclass(frozen=True)
class Sample:
    image_hash: int
    bounds: tuple[int, int, int, int]  # left, top, right, bottom, exclusive
    text: str


# Frame labels can be read unambiguously, including all 26 Latin capitals.
# The body labels additionally supply J/K/Q/W/Y/Z and some lowercase forms.
# See docs/gui-assets.md for the independent screenshot references.
ORIGINAL_SAMPLES = (
    Sample(0x94eb76bc, (14, 8, 101, 18), 'IN GAME OPTIONS'),
    Sample(0x94eb76bc, (332, 33, 411, 43), 'REALISM MENU'),
    Sample(0x94eb76bc, (332, 206, 440, 216), 'EXIT OPTIONS'),
    Sample(0x94eb76bc, (95, 239, 190, 249), 'FINE TUNE OPTIONS'),
    Sample(0x94eb76bc, (244, 49, 303, 59), 'Joystick'),
    Sample(0x94eb76bc, (241, 65, 303, 75), 'Keyboard'),
    Sample(0x94eb76bc, (270, 81, 301, 91), 'Yes'),
    Sample(0x94eb76bc, (332, 50, 400, 60), 'World Detail'),
    Sample(0x94eb76bc, (332, 258, 355, 269), 'Quit'),
    Sample(0x950b6052, (330, 312, 456, 322), 'BILDSCHIRM-AUSSCHNITT'),
    Sample(0x950b6052, (330, 206, 452, 216), 'OPTIONEN VERLASSEN'),
    Sample(0x950b6052, (333, 240, 338, 250), 'Z'),
)


@dataclass(frozen=True)
class Glyph:
    width: int
    height: int
    mask: bytes


def bright(image, x, y):
    color_index = image.indexed[y * image.width + x]
    rgb = image.palette[color_index * 3:color_index * 3 + 3]
    return min(rgb) > WHITE_THRESHOLD


def sample_glyphs(image, bounds, text):
    """Segment one labeled text line by fully empty columns.

    A mismatch is an error rather than guessing a glyph-to-character map.
    """
    left, top, right, bottom = bounds
    if not (0 <= left < right <= image.width and
            0 <= top < bottom <= image.height and bottom - top <= CELL_HEIGHT):
        raise ValueError('invalid menu font sample rectangle')
    columns = [x for x in range(left, right)
               if any(bright(image, x, y) for y in range(top, bottom))]
    groups = []
    for x in columns:
        if not groups or x > groups[-1][1] + 1:
            groups.append([x, x])
        else:
            groups[-1][1] = x
    expected = text.replace(' ', '')
    if len(groups) != len(expected):
        raise ValueError(f'font sample {text!r}: {len(groups)} groups for '
                         f'{len(expected)} characters')
    found = []
    for char, (start, end) in zip(expected, groups):
        width, height = end - start + 1, bottom - top
        if width > CELL_WIDTH:
            raise ValueError(f'font glyph {char!r} wider than atlas cell')
        mask = bytes(int(bright(image, x, y))
                     for y in range(top, bottom) for x in range(start, end + 1))
        found.append((char, Glyph(width, height, mask)))
    return found


def collect_glyphs(images, samples, required=string.ascii_uppercase):
    variants = defaultdict(list)
    for sample in samples:
        try:
            image = images[sample.image_hash]
        except KeyError as error:
            raise ValueError(f'missing font reference image {sample.image_hash:08x}') from error
        for char, glyph in sample_glyphs(image, sample.bounds, sample.text):
            variants[char].append(glyph)
    # Background/palette differences sometimes change one bright edge pixel.
    # Keep the most commonly observed raster for each letter and record every
    # disagreement; never silently combine unlike letter shapes.
    glyphs = {char: Counter(candidates).most_common(1)[0][0]
              for char, candidates in variants.items()}
    conflicts = sorted(char for char, candidates in variants.items()
                       if len(set(candidates)) > 1)
    missing = sorted(set(required) - set(glyphs))
    if missing:
        raise ValueError('missing required font letters: ' + ''.join(missing))
    return glyphs, conflicts


def render_atlas(glyphs):
    """8x12-cell magenta/white atlas for SDL_LoadBMP + colour key.

    The adjacent plain text widths file contains 95 cell ink widths in ASCII
    order, -1 for unavailable glyphs and 0 for the blank space character.
    """
    columns = 16
    rows = (ASCII_END - ASCII_START + columns) // columns
    width, height = CELL_WIDTH * columns, CELL_HEIGHT * rows
    indexed = bytearray(width * height)
    widths = []
    for code in range(ASCII_START, ASCII_END + 1):
        char = chr(code)
        glyph = glyphs.get(char)
        if char == ' ':
            widths.append(0)
        elif glyph is None:
            widths.append(-1)
        else:
            if glyph.height > CELL_HEIGHT or glyph.width > CELL_WIDTH:
                raise ValueError('menu font glyph does not fit atlas cell')
            widths.append(glyph.width)
            left = (code - ASCII_START) % columns * CELL_WIDTH
            top = (code - ASCII_START) // columns * CELL_HEIGHT
            for y in range(glyph.height):
                for x in range(glyph.width):
                    indexed[(top + y) * width + left + x] = glyph.mask[y * glyph.width + x]
    palette = b'\xff\x00\xff\xff\xff\xff' + bytes(768 - 6)
    return Image(width, height, bytes(indexed), palette), widths


def rip_font(game_dir, out_dir):
    archive = DIDArchive.from_path(Path(game_dir) / 'DID1ST.DAT')
    images = {}
    for key in {sample.image_hash for sample in ORIGINAL_SAMPLES}:
        entry = archive.by_hash.get(key)
        if not entry:
            raise ValueError(f'DID1ST.DAT is missing screenshot {key:08x}')
        images[key] = decode(archive.member(entry))
    glyphs, conflicts = collect_glyphs(images, ORIGINAL_SAMPLES)
    atlas, widths = render_atlas(glyphs)
    output = Path(out_dir)
    files = {
        'menu-font.bmp': encode_bmp(atlas),
        'menu-font.png': encode_png(atlas),
        'menu-font.widths': (' '.join(map(str, widths)) + '\n').encode('ascii'),
        'menu-font.json': (json.dumps({
            'source': 'DID1ST.DAT',
            'character_range': 'ASCII 32..126',
            'cell_size': [CELL_WIDTH, CELL_HEIGHT],
            'recovered_characters': ''.join(sorted(ch for ch in glyphs if ch in string.printable)),
            'conflicting_samples': conflicts,
            'sample_count': len(ORIGINAL_SAMPLES),
        }, indent=2, ensure_ascii=False) + '\n').encode('utf-8'),
    }
    output.mkdir(parents=True, exist_ok=True)
    for name in files:
        if (output / name).exists():
            raise FileExistsError(f'refusing to overwrite {output / name}')
    for name, data in files.items():
        with (output / name).open('xb') as stream:
            stream.write(data)
    return glyphs, conflicts
