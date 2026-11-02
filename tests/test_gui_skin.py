# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Reconstruction tests using artificial indexed samples and optional original data."""

import json
import os
from pathlib import Path
import tempfile
import unittest

from tools.gui_skin import SKIN_IMAGES, keyed_units, rip_skin, skin_exports
from tools.ilbm_image import Image


def sample_image(width, height, *, units=False):
    indexed = bytearray(width * height)
    if units:
        indexed[1] = 1
        indexed[2] = 2
    palette = bytearray(768)
    palette[:3] = b'\x93\x00\x00'
    palette[3:6] = b'\xff\x00\x00'
    palette[6:9] = b'\x00\x00\xff'
    return Image(width, height, bytes(indexed), bytes(palette))


class GuiSkinTests(unittest.TestCase):
    def test_index_zero_transparent_without_erasing_red_foreground(self):
        image = sample_image(640, 480, units=True)
        png, bmp = keyed_units(image)
        self.assertEqual(png.alpha[:3], b'\x00\xff\xff')
        self.assertEqual(bmp.palette[:3], b'\xff\x00\xff')
        self.assertEqual(bmp.palette[3:6], b'\xff\x00\x00')
        self.assertEqual(png.palette[3:6], b'\xff\x00\x00')
        with self.assertRaisesRegex(ValueError, 'dimensions'):
            keyed_units(sample_image(320, 200))

    def test_exports_texture_manifest_without_external_assets(self):
        images = {hashed: sample_image(*dimensions, units=name == 'units')
                  for name, (hashed, dimensions) in SKIN_IMAGES.items()}
        files, manifest = skin_exports(images)
        self.assertEqual(set(manifest['textures']), set(SKIN_IMAGES))
        self.assertEqual(manifest['textures']['map']['dimensions'], [400, 400])
        self.assertEqual(manifest['textures']['units']['hash'], 'ade5ec9a')
        self.assertEqual(files['skin-units.png'][:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(files['skin-units.bmp'][:2], b'BM')
        self.assertEqual(files['skin-title.bmp'][:2], b'BM')
        self.assertNotIn('skin-map.bmp', files)
        self.assertEqual(json.loads(files['skin.json']), manifest)
        with self.assertRaisesRegex(ValueError, 'missing original skin image'):
            skin_exports({})


@unittest.skipUnless(os.environ.get('EF2000_DATA_DIR'),
                     'set EF2000_DATA_DIR for original GUI skin extraction')
class OriginalGuiSkinTests(unittest.TestCase):
    def test_archive_exports_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = rip_skin(Path(os.environ['EF2000_DATA_DIR']), directory)
            self.assertEqual(len(manifest['textures']), 5)
            self.assertEqual(manifest['textures']['title']['hash'], 'ad224253')
            self.assertEqual((Path(directory) / 'skin-panel.bmp').read_bytes()[:2], b'BM')
            self.assertEqual((Path(directory) / 'skin-units.png').read_bytes()[:8],
                             b'\x89PNG\r\n\x1a\n')
            self.assertEqual(json.loads((Path(directory) / 'skin.json').read_text()), manifest)
            with self.assertRaises(FileExistsError):
                rip_skin(Path(os.environ['EF2000_DATA_DIR']), directory)
