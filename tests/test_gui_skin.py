# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Reconstruction tests using artificial indexed samples and optional original data."""

import json
import os
from pathlib import Path
import tempfile
import unittest

from tools.gui_skin import (SKIN_IMAGES, UNIT_SYMBOLS, keyed_units,
                            rip_skin, skin_exports, unit_candidates)
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
        self.assertEqual(unit_candidates(image),
                         [{'row': 0, 'rect': [1, 0, 2, 1]}])
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
        self.assertEqual(json.loads(files['skin-units-candidates.json'])['candidates'],
                         [{'row': 0, 'rect': [1, 0, 2, 1]}])
        symbols = json.loads(files['skin-units-symbols.json'])['symbols']
        self.assertEqual(symbols['map_aircraft_red'], [601, 1, 19, 17])
        self.assertEqual(symbols['flag_russia_waving'], [42, 185, 13, 12])
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
            candidates = json.loads((Path(directory) / 'skin-units-candidates.json').read_text())
            self.assertGreater(len(candidates['candidates']), 100)
            from tools.did_archive import DIDArchive
            from tools.ilbm_image import decode
            archive = DIDArchive.from_path(Path(os.environ['EF2000_DATA_DIR']) / 'DID1ST.DAT')
            image = decode(archive.member(archive.by_hash[SKIN_IMAGES['units'][0]]))
            for name, (x, y, width, height) in UNIT_SYMBOLS.items():
                self.assertTrue(any(any(image.indexed[row * image.width + x:
                                                      row * image.width + x + width])
                                    for row in range(y, y + height)), name)
            with self.assertRaises(FileExistsError):
                rip_skin(Path(os.environ['EF2000_DATA_DIR']), directory)

    def test_skin_cli_includes_original_font_for_runtime(self):
        from contextlib import redirect_stdout
        from io import StringIO
        from tools.ef2000_gui_assets import main
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'skin'
            with redirect_stdout(StringIO()):
                main(['skin', os.environ['EF2000_DATA_DIR'], '--out', str(output)])
            self.assertEqual((output / 'menu-font.bmp').read_bytes()[:2], b'BM')
            self.assertEqual(len((output / 'menu-font.widths').read_text().split()), 95)
            self.assertTrue((output / 'skin-title.bmp').is_file())
