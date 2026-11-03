#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Recover a proportional font from the original TACTCOM font specimen.

DID1ST.DAT hash 0x33773001 contains a 320x200 font specimen with consecutive
ASCII characters ! through z, followed by accented glyphs whose character
mapping remains to be established. Most game menu text is baked into images;
this font is useful for the few dynamic labels and for the native overlay.
No original image or font data lives in Git.
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
FONT_SHEET_HASH = 0x33773001


@dataclass(frozen=True)
class Sample:
    image_hash: int
    bounds: tuple[int, int, int, int]  # left, top, right, bottom, exclusive
    text: str
    merge_groups: tuple[int, ...] = ()  # index of first group to merge with next


# The first specimen row contains ASCII 33..76, the second 77..119, and
# the third starts with 120..122 before switching to accented characters.
# The double quote has two disconnected vertical strokes, separated by one
# blank column, so its two bright-column groups must be joined explicitly.
SHEET_SAMPLES = (
    Sample(FONT_SHEET_HASH, (0, 32, 256, 44),
           ''.join(map(chr, range(33, 77))), merge_groups=(1,)),
    Sample(FONT_SHEET_HASH, (0, 46, 256, 58),
           ''.join(map(chr, range(77, 120)))),
    Sample(FONT_SHEET_HASH, (0, 60, 16, 72), 'xyz'),
)


# Earlier work also measured already-rendered option-screen labels. Retain
# these independent comparison points; they are NOT the primary font source.
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


@dataclass(frozen=True)
class UnmappedGlyph:
    bounds: tuple[int, int, int, int]
    glyph: Glyph


def bright(image, x, y):
    color_index = image.indexed[y * image.width + x]
    rgb = image.palette[color_index * 3:color_index * 3 + 3]
    return min(rgb) > WHITE_THRESHOLD


def lit_column_groups(image, bounds):
    """Return inclusive bright-column spans within a bounded source line."""
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
    return groups


def glyph_from_span(image, top, bottom, start, end):
    width, height = end - start + 1, bottom - top
    if width > CELL_WIDTH:
        raise ValueError('font glyph wider than atlas cell')
    return Glyph(width, height, bytes(int(bright(image, x, y))
                                     for y in range(top, bottom)
                                     for x in range(start, end + 1)))


def sample_glyphs(image, bounds, text, merge_groups=()):
    """Segment one labeled line, rejecting speculative glyph mappings."""
    _, top, _, bottom = bounds
    groups = lit_column_groups(image, bounds)
    for index in sorted(merge_groups, reverse=True):
        if not 0 <= index < len(groups) - 1:
            raise ValueError('invalid font sample group merge index')
        if groups[index + 1][0] - groups[index][1] != 2:
            raise ValueError('font sample merge must bridge one blank column')
        groups[index][1] = groups[index + 1][1]
        del groups[index + 1]
    expected = text.replace(' ', '')
    if len(groups) != len(expected):
        raise ValueError(f'font sample {text!r}: {len(groups)} groups for '
                         f'{len(expected)} characters')
    found = []
    for char, (start, end) in zip(expected, groups):
        found.append((char, glyph_from_span(image, top, bottom, start, end)))
    return found


# The rest of the third text row and fourth row contain 42 + 6 visually
# distinct accented symbols. We have NOT established their Unicode/CP437
# values. The extraction preserves order and image coordinates only.
UNMAPPED_BANDS = (
    ((17, 60, 250, 72), 42),
    ((0, 74, 44, 86), 6),
)

# Independently confirmed by pixel-perfect comparison against the original
# German TACTCOM title "Einsatzübersicht" at screenshot coordinate (104,160).
# Do not extrapolate the neighboring codepage positions from this one match.
KNOWN_EXTENDED_GLYPHS = {16: 'ü'}


def collect_unmapped_glyphs(image):
    found = []
    for bounds, expected in UNMAPPED_BANDS:
        _, top, _, bottom = bounds
        groups = lit_column_groups(image, bounds)
        if len(groups) != expected:
            raise ValueError(f'unmapped font band {top}: {len(groups)} groups '
                             f'for {expected} expected characters')
        for start, end in groups:
            found.append(UnmappedGlyph((start, top, end + 1, bottom),
                                       glyph_from_span(image, top, bottom, start, end)))
    return found


def render_unmapped_atlas(unmapped):
    columns = 16
    width = columns * CELL_WIDTH
    height = ((len(unmapped) + columns - 1) // columns) * CELL_HEIGHT
    indexed = bytearray(width * height)
    for index, item in enumerate(unmapped):
        glyph = item.glyph
        if glyph.height > CELL_HEIGHT or glyph.width > CELL_WIDTH:
            raise ValueError('unmapped font glyph does not fit atlas cell')
        x0 = (index % columns) * CELL_WIDTH
        y0 = (index // columns) * CELL_HEIGHT
        for y in range(glyph.height):
            for x in range(glyph.width):
                indexed[(y0 + y) * width + x0 + x] = glyph.mask[y * glyph.width + x]
    palette = b'\xff\x00\xff\xff\xff\xff' + bytes(768 - 6)
    return Image(width, height, bytes(indexed), palette)


def unmapped_gallery(unmapped):
    """JavaScript-free, nearest-neighbor 4x gallery with source positions."""
    cards = []
    for index, item in enumerate(unmapped):
        left, top, right, bottom = item.bounds
        x, y = (index % 16) * 32, (index // 16) * 48
        cards.append(f'<figure><div class="glyph" style="background-position:-{x}px -{y}px"></div>'
                     f'<figcaption>#{index:02d} ({left},{top})–({right},{bottom})</figcaption>'
                     '</figure>')
    return ('<!doctype html><html lang="en"><meta charset="utf-8">'
            '<title>EF2000 unmapped font specimen glyphs</title><style>'
            'body{font:14px sans-serif;background:#1c222a;color:#eee;margin:2em}'
            'main{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:12px}'
            'figure{margin:0;padding:12px;background:#303842;border:1px solid #666}'
            '.glyph{width:32px;height:48px;background-image:url(menu-font-extended.png);'
            'background-size:512px 144px;image-rendering:pixelated;background-repeat:no-repeat}'
            'figcaption{font:12px monospace;margin-top:8px}</style><h1>Unmapped glyphs</h1>'
            '<p>Source DID1ST 0x33773001. Index is not a character code;'
            ' no Unicode mapping is asserted.</p><main>' + ''.join(cards) + '</main></html>\n')


def collect_glyphs(images, samples, required=string.ascii_uppercase):
    variants = defaultdict(list)
    for sample in samples:
        try:
            image = images[sample.image_hash]
        except KeyError as error:
            raise ValueError(f'missing font reference image {sample.image_hash:08x}') from error
        for char, glyph in sample_glyphs(image, sample.bounds, sample.text,
                                        sample.merge_groups):
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
    entry = archive.by_hash.get(FONT_SHEET_HASH)
    if not entry:
        raise ValueError(f'DID1ST.DAT is missing font specimen {FONT_SHEET_HASH:08x}')
    image = decode(archive.member(entry))
    if (image.width, image.height) != (320, 200):
        raise ValueError('unexpected TACTCOM font specimen dimensions')
    required = ''.join(map(chr, range(33, 123)))
    glyphs, conflicts = collect_glyphs({FONT_SHEET_HASH: image}, SHEET_SAMPLES,
                                       required=required)
    atlas, widths = render_atlas(glyphs)
    unmapped = collect_unmapped_glyphs(image)
    extended_atlas = render_unmapped_atlas(unmapped)
    extended_manifest = {
        'source': 'DID1ST.DAT',
        'source_hash': f'{FONT_SHEET_HASH:08x}',
        'cell_size': [CELL_WIDTH, CELL_HEIGHT],
        'notes': 'Source order retained; only screenshot-verified Unicode glyphs are assigned',
        'glyphs': [
            {'index': index, 'bounds': list(item.bounds),
             'width': item.glyph.width, 'unicode': KNOWN_EXTENDED_GLYPHS.get(index)}
            for index, item in enumerate(unmapped)
        ],
    }
    output = Path(out_dir)
    files = {
        'menu-font.bmp': encode_bmp(atlas),
        'menu-font.png': encode_png(atlas),
        'menu-font.widths': (' '.join(map(str, widths)) + '\n').encode('ascii'),
        'menu-font-extended.png': encode_png(extended_atlas),
        'menu-font-extended.bmp': encode_bmp(extended_atlas),
        'menu-font-extended.json': (json.dumps(extended_manifest, indent=2) + '\n').encode('utf-8'),
        'menu-font-extended.html': unmapped_gallery(unmapped).encode('utf-8'),
        'menu-font.json': (json.dumps({
            'source': 'DID1ST.DAT',
            'source_hash': f'{FONT_SHEET_HASH:08x}',
            'source_description': '320x200 original font specimen',
            'character_range': 'ASCII 32..126',
            'unmapped_ascii': '{|}~',
            'cell_size': [CELL_WIDTH, CELL_HEIGHT],
            'recovered_characters': ''.join(sorted(ch for ch in glyphs if ch in string.printable)),
            'conflicting_samples': conflicts,
            'sample_count': len(SHEET_SAMPLES),
            'unmapped_extended_glyphs': len(unmapped),
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
