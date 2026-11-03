# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Menu font segmentation, atlas layout and original-asset opt-in replay."""

import json
import os
from pathlib import Path
import string
import tempfile
import unittest

from tools.ilbm_image import Image
from tools.menu_font import (ASCII_END, ASCII_START, CELL_HEIGHT, CELL_WIDTH,
                             FONT_SHEET_HASH, Glyph, Sample, collect_glyphs,
                             collect_unmapped_glyphs, render_atlas,
                             render_unmapped_atlas, rip_font, sample_glyphs,
                             unmapped_gallery)


def synthetic_image():
    # Two three-pixel-wide letters separated by exactly two empty columns.
    width, height = 10, 10
    pixels = bytearray(width * height)
    for y in range(height):
        for x in (1, 3, 6, 8):
            pixels[y * width + x] = 1
    pixels[1 * width + 2] = 1
    pixels[1 * width + 7] = 1
    return Image(width, height, bytes(pixels),
                 b'\x00\x00\x00\xff\xff\xff' + bytes(762))


class MenuFontTests(unittest.TestCase):
    def test_labeled_column_segmentation_and_unknown_glyph_rejection(self):
        image = synthetic_image()
        entries = sample_glyphs(image, (0, 0, 10, 10), 'A B')
        self.assertEqual([char for char, _ in entries], ['A', 'B'])
        self.assertEqual([glyph.width for _, glyph in entries], [3, 3])
        self.assertEqual(entries[0][1].mask[:3], b'\x01\x00\x01')
        with self.assertRaisesRegex(ValueError, 'groups for 3 characters'):
            sample_glyphs(image, (0, 0, 10, 10), 'ABC')
        with self.assertRaisesRegex(ValueError, 'rectangle'):
            sample_glyphs(image, (0, 0, 10, 13), 'AB')
        sample = Sample(1234, (0, 0, 10, 10), 'AB')
        glyphs, conflicts = collect_glyphs({1234: image}, [sample], required='AB')
        self.assertEqual(set(glyphs), {'A', 'B'})
        self.assertEqual(conflicts, [])
        with self.assertRaisesRegex(ValueError, 'missing required font letters'):
            collect_glyphs({1234: image}, [sample], required='ABC')

    def test_conflicting_rasters_use_majority_without_merging(self):
        original = synthetic_image()
        modified = bytearray(original.indexed)
        modified[2 * original.width + 2] = 1
        images = {1: original, 2: Image(original.width, original.height,
                                        bytes(modified), original.palette)}
        samples = [Sample(key, (0, 0, 10, 10), 'AB') for key in (1, 2, 1)]
        result, conflicts = collect_glyphs(images, samples, required='AB')
        self.assertEqual(conflicts, ['A'])
        self.assertEqual(result['A'], sample_glyphs(original, (0, 0, 10, 10), 'AB')[0][1])

    def test_disconnected_strokes_are_merged_only_when_declared(self):
        image = Image(6, 3, bytes([0, 1, 0, 1, 0, 0] * 3),
                      b'\x00\x00\x00\xff\xff\xff' + bytes(762))
        with self.assertRaisesRegex(ValueError, '2 groups for 1 characters'):
            sample_glyphs(image, (0, 0, 6, 3), '"')
        glyphs = sample_glyphs(image, (0, 0, 6, 3), '"', merge_groups=(0,))
        self.assertEqual(glyphs[0], ('"', Glyph(3, 3, b'\x01\x00\x01' * 3)))
        with self.assertRaisesRegex(ValueError, 'merge index'):
            sample_glyphs(image, (0, 0, 6, 3), '"', merge_groups=(2,))

    def test_atlas_layout_and_missing_widths(self):
        glyphs = {'A': Glyph(3, 2, b'\x01\x00\x01\x00\x01\x00'),
                  'M': Glyph(7, 1, b'\x01' * 7)}
        atlas, widths = render_atlas(glyphs)
        self.assertEqual((atlas.width, atlas.height), (128, 72))
        self.assertEqual(len(widths), ASCII_END - ASCII_START + 1)
        self.assertEqual(widths[ord(' ') - ASCII_START], 0)
        self.assertEqual(widths[ord('A') - ASCII_START], 3)
        self.assertEqual(widths[ord('M') - ASCII_START], 7)
        self.assertEqual(widths[ord('C') - ASCII_START], -1)
        left = (ord('A') - ASCII_START) % 16 * CELL_WIDTH
        top = (ord('A') - ASCII_START) // 16 * CELL_HEIGHT
        self.assertEqual(atlas.indexed[top * atlas.width + left], 1)
        self.assertEqual(atlas.indexed[top * atlas.width + left + 1], 0)
        self.assertEqual(atlas.palette[:6], b'\xff\x00\xff\xff\xff\xff')

    def test_unmapped_glyphs_are_numbered_without_guessing_codepoints(self):
        pixels = bytearray(320 * 200)
        for x in range(17, 17 + 42 * 5, 5):
            pixels[60 * 320 + x] = 1
        for x in range(0, 6 * 5, 5):
            pixels[74 * 320 + x] = 1
        palette = b'\x00\x00\x00\xff\xff\xff' + bytes(762)
        image = Image(320, 200, bytes(pixels), palette)
        unmapped = collect_unmapped_glyphs(image)
        self.assertEqual(len(unmapped), 48)
        self.assertEqual(unmapped[0].bounds, (17, 60, 18, 72))
        self.assertEqual(unmapped[41].bounds, (222, 60, 223, 72))
        self.assertEqual(unmapped[42].bounds, (0, 74, 1, 86))
        self.assertEqual(unmapped[47].bounds, (25, 74, 26, 86))
        atlas = render_unmapped_atlas(unmapped)
        self.assertEqual((atlas.width, atlas.height), (128, 36))
        self.assertEqual(atlas.indexed[0], 1)
        self.assertEqual(atlas.indexed[12 * atlas.width], 1)
        html = unmapped_gallery(unmapped)
        self.assertEqual(html.count('<figure>'), 48)
        self.assertIn('Index is not a character code', html)
        from tools.menu_font import KNOWN_EXTENDED_GLYPHS
        self.assertEqual(KNOWN_EXTENDED_GLYPHS, {16: 'ü'})
        damaged = bytearray(pixels)
        damaged[60 * 320 + 17] = 0
        with self.assertRaisesRegex(ValueError, '41 groups for 42 expected'):
            collect_unmapped_glyphs(Image(320, 200, bytes(damaged), palette))


@unittest.skipUnless(os.environ.get('EF2000_DATA_DIR'),
                     'set EF2000_DATA_DIR to rip supplied TACTCOM menu font')
class OriginalMenuFontTests(unittest.TestCase):
    def test_original_font_specimen_recovers_digits_lowercase_and_uppercase(self):
        with tempfile.TemporaryDirectory() as directory:
            glyphs, conflicts = rip_font(Path(os.environ['EF2000_DATA_DIR']),
                                        Path(directory))
            self.assertEqual(len(glyphs), 90)
            self.assertTrue(set(string.ascii_letters + string.digits).issubset(glyphs))
            self.assertEqual(conflicts, [])  # direct font specimen, not screen variants
            widths = [int(value) for value in
                      (Path(directory) / 'menu-font.widths').read_text().split()]
            self.assertEqual(len(widths), 95)
            self.assertEqual(widths[ord('M') - 32], 7)
            self.assertEqual(widths[ord('"') - 32], 3)
            self.assertEqual(widths[ord('0') - 32], 4)
            self.assertEqual(widths[ord('z') - 32], 3)
            self.assertEqual(widths[ord('~') - 32], -1)
            self.assertEqual(widths[ord('{') - 32], -1)
            self.assertTrue((Path(directory) / 'menu-font.bmp').read_bytes().startswith(b'BM'))
            manifest = json.loads((Path(directory) / 'menu-font.json').read_text())
            self.assertEqual(manifest['cell_size'], [8, 12])
            self.assertEqual(manifest['source_hash'], f'{FONT_SHEET_HASH:08x}')
            self.assertEqual(manifest['unmapped_ascii'], '{|}~')
            self.assertEqual(manifest['sample_count'], 3)
            self.assertEqual(manifest['unmapped_extended_glyphs'], 48)
            extended = json.loads((Path(directory) / 'menu-font-extended.json').read_text())
            self.assertEqual(len(extended['glyphs']), 48)
            self.assertEqual(extended['glyphs'][0]['bounds'], [17, 60, 21, 72])
            self.assertEqual(extended['glyphs'][-1]['bounds'], [40, 74, 44, 86])
            self.assertEqual(extended['glyphs'][16]['unicode'], 'ü')
            self.assertTrue(all(item['unicode'] is None
                                for idx, item in enumerate(extended['glyphs']) if idx != 16))
            self.assertTrue((Path(directory) / 'menu-font-extended.png').read_bytes().startswith(
                b'\x89PNG\r\n\x1a\n'))
            self.assertTrue((Path(directory) / 'menu-font-extended.bmp').read_bytes().startswith(b'BM'))
            with self.assertRaises(FileExistsError):
                rip_font(Path(os.environ['EF2000_DATA_DIR']), Path(directory))


if __name__ == '__main__':
    unittest.main()
