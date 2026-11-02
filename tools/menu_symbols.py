#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Extract the small two-row symbol strip from TACTCOM's font specimen.

The 320x200 image at DID1ST hash 0x33773001 places seven 16x16 symbol
cells in each of two rows. Preserve their colours and masks verbatim;
the symbols are *not yet proven* to be the actual GUI button artwork.
The adjacent pointer-like drawing is deliberately excluded because it
overlaps the cell grid.
"""

import json
from pathlib import Path

try:
    from .did_archive import DIDArchive
    from .ilbm_image import Image, decode, encode_bmp, encode_png
    from .menu_font import FONT_SHEET_HASH
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import Image, decode, encode_bmp, encode_png
    from menu_font import FONT_SHEET_HASH


CELL_SIZE = 16
SYMBOL_NAMES = ('arrow_up', 'arrow_down', 'arrow_left', 'arrow_right',
                'cross', 'overlap', 'corners')
ROW_NAMES = ('light', 'dark')
ATLAS_WIDTH = CELL_SIZE * len(SYMBOL_NAMES)
ATLAS_HEIGHT = CELL_SIZE * len(ROW_NAMES)


def symbol_atlases(image):
    """Return magenta-keyed BMP and transparent PNG source images."""
    if (image.width, image.height) != (320, 200):
        raise ValueError('unexpected TACTCOM symbol specimen dimensions')
    if len(image.indexed) != image.width * image.height or len(image.palette) != 768:
        raise ValueError('invalid symbol specimen palette or pixels')
    pixels = bytearray()
    for y in range(ATLAS_HEIGHT):
        start = y * image.width
        pixels.extend(image.indexed[start:start + ATLAS_WIDTH])
    alpha = bytes(0 if index == 0 else 255 for index in pixels)
    keyed_palette = b'\xff\x00\xff' + image.palette[3:]
    return (Image(ATLAS_WIDTH, ATLAS_HEIGHT, bytes(pixels), keyed_palette),
            Image(ATLAS_WIDTH, ATLAS_HEIGHT, bytes(pixels), image.palette, alpha))


def rip_symbols(game_dir, out_dir):
    """Save the optional overlay symbols without modifying original assets."""
    archive = DIDArchive.from_path(Path(game_dir) / 'DID1ST.DAT')
    entry = archive.by_hash.get(FONT_SHEET_HASH)
    if not entry:
        raise ValueError(f'DID1ST.DAT is missing symbol specimen {FONT_SHEET_HASH:08x}')
    image = decode(archive.member(entry))
    keyed, transparent = symbol_atlases(image)
    metadata = {
        'source': 'DID1ST.DAT',
        'source_hash': f'{FONT_SHEET_HASH:08x}',
        'cell_size': CELL_SIZE,
        'size': [ATLAS_WIDTH, ATLAS_HEIGHT],
        'notes': 'Original specimen symbols; menu button use is unverified',
        'cells': [
            {'name': f'{name}_{variant}',
             'x': column * CELL_SIZE, 'y': row * CELL_SIZE,
             'width': CELL_SIZE, 'height': CELL_SIZE}
            for row, variant in enumerate(ROW_NAMES)
            for column, name in enumerate(SYMBOL_NAMES)
        ],
    }
    files = {
        'menu-symbols.bmp': encode_bmp(keyed),
        'menu-symbols.png': encode_png(transparent),
        'menu-symbols.json': (json.dumps(metadata, indent=2) + '\n').encode('utf-8'),
    }
    output = Path(out_dir)
    output.mkdir(parents=True, exist_ok=True)
    for name in files:
        if (output / name).exists():
            raise FileExistsError(f'refusing to overwrite {output / name}')
    for name, content in files.items():
        with (output / name).open('xb') as stream:
            stream.write(content)
    return metadata
