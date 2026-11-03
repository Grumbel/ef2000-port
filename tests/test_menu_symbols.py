# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free original-symbol extraction and optional DID1ST replay."""

import json
import os
from pathlib import Path
import tempfile
import unittest

from tools.ilbm_image import Image, encode_png
from tools.menu_symbols import (ATLAS_HEIGHT, ATLAS_WIDTH, BUTTON_SHAPES, CELL_SIZE,
                                ROW_NAMES, SYMBOL_NAMES, rip_symbols,
                                symbol_atlases)


class MenuSymbolTests(unittest.TestCase):
    def test_window_glyph_bounds_do_not_follow_nominal_cell_grid(self):
        self.assertEqual(BUTTON_SHAPES['close'], (64, 0, 12, 14))
        self.assertEqual(BUTTON_SHAPES['maximize'], (76, 0, 16, 14))
        for x, y, width, height in BUTTON_SHAPES.values():
            self.assertLessEqual(x + width, ATLAS_WIDTH)
            self.assertLessEqual(y + height, CELL_SIZE)
        self.assertEqual(BUTTON_SHAPES['close'][0] + BUTTON_SHAPES['close'][2],
                         BUTTON_SHAPES['maximize'][0])

    def test_keyed_bmp_and_transparent_png_preserve_sampled_colours(self):
        pixels = bytearray(320 * 200)
        pixels[1 * 320 + 1] = 15       # bright arrow
        pixels[17 * 320 + 65] = 8      # dark cross
        pixels[20 * 320 + 129] = 15    # excluded overlapping drawing
        palette = bytearray(768)
        palette[:3] = b'\x00\x53\xa3'
        palette[8 * 3:8 * 3 + 3] = b'\x00\x00\x00'
        palette[15 * 3:15 * 3 + 3] = b'\xff\xff\xff'
        keyed, transparent = symbol_atlases(Image(320, 200, bytes(pixels), bytes(palette)))
        self.assertEqual((keyed.width, keyed.height), (ATLAS_WIDTH, ATLAS_HEIGHT))
        self.assertEqual(ATLAS_WIDTH, len(SYMBOL_NAMES) * CELL_SIZE)
        self.assertEqual(ATLAS_HEIGHT, len(ROW_NAMES) * CELL_SIZE)
        self.assertEqual(keyed.palette[:3], b'\xff\x00\xff')
        self.assertEqual(keyed.indexed[1 * ATLAS_WIDTH + 1], 15)
        self.assertEqual(keyed.indexed[17 * ATLAS_WIDTH + 65], 8)
        self.assertEqual(transparent.alpha[0], 0)
        self.assertEqual(transparent.alpha[1 * ATLAS_WIDTH + 1], 255)
        self.assertEqual(transparent.alpha[17 * ATLAS_WIDTH + 65], 255)
        self.assertEqual(encode_png(transparent)[25], 6)  # RGBA PNG
        with self.assertRaisesRegex(ValueError, 'dimensions'):
            symbol_atlases(Image(112, 32, bytes(112 * 32), bytes(palette)))


@unittest.skipUnless(os.environ.get('EF2000_DATA_DIR'),
                     'set EF2000_DATA_DIR for original symbol specimen extraction')
class OriginalMenuSymbolTests(unittest.TestCase):
    def test_original_archive_exports_all_named_tiles_without_overwriting(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = rip_symbols(Path(os.environ['EF2000_DATA_DIR']), Path(directory))
            self.assertEqual(len(metadata['cells']), 14)
            self.assertEqual(metadata['size'], [112, 32])
            self.assertEqual(metadata['source_hash'], '33773001')
            self.assertEqual(metadata['cells'][11]['name'], 'cross_dark')
            self.assertEqual(metadata['candidate_button_shapes']['close_dark'],
                             [64, 16, 12, 14])
            self.assertEqual(metadata['candidate_button_shapes']['maximize_dark'],
                             [76, 16, 16, 14])
            self.assertEqual((Path(directory) / 'menu-symbols.bmp').read_bytes()[:2], b'BM')
            self.assertEqual((Path(directory) / 'menu-symbols.png').read_bytes()[:8],
                             b'\x89PNG\r\n\x1a\n')
            self.assertEqual(json.loads((Path(directory) / 'menu-symbols.json').read_text()),
                             metadata)
            with self.assertRaises(FileExistsError):
                rip_symbols(Path(os.environ['EF2000_DATA_DIR']), Path(directory))


if __name__ == '__main__':
    unittest.main()
