# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Original-asset-free checks for hashed archives, menu scripts and IFF art."""

import json
import hashlib
import os
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from tools.did_archive import DIDArchive, hash_path
from tools.ef2000_gui_assets import extract, gallery, inspect_script, inventory, safe_relative
from tools.ilbm_image import Image, byterun_row, chunks, decode, encode_png, encode_bmp
from tools.pcx_image import decode as decode_pcx, is_pcx
from tools.bmp_image import decode as decode_bmp, is_bmp
from tools.ra_decode import decompress
from tools.resolve_archive_trace import parse_request, normalize_path, resolve_request
from tools.probe_did_grids import GRID_BYTES, GRID_PREFIX, audit as grid_audit, grid_candidate, preview_png
from tools.spr_image import decode as decode_spr


def chunk(kind, contents):
    return kind + struct.pack('>I', len(contents)) + contents + (b'\0' if len(contents) % 2 else b'')


def image_fixture(*, pixels=(0, 1), form=b'ILBM', compressed=True, transparent=None):
    mask = 2 if transparent is not None else 0
    bmhd = struct.pack('>HHhhBBBBHBBhh', 2, 1, 0, 0, 1 if form == b'ILBM' else 8,
                       mask, int(compressed), 0, transparent or 0, 1, 1, 2, 1)
    cmap = b'\0\0\0\xff\x80\x40' + (b'\0\0\0' * 254)
    raw = bytes([0x40, 0]) if form == b'ILBM' else bytes(pixels)
    body = bytes([len(raw) - 1]) + raw if compressed else raw
    contents = form + chunk(b'BMHD', bmhd) + chunk(b'CMAP', cmap) + chunk(b'BODY', body)
    return b'FORM' + struct.pack('>I', len(contents)) + contents


def bmp_fixture(*, top_down=False):
    """A 3x2, 8-bit BMP with an intentionally padded bottom-up pixel array."""
    width, height = 3, -2 if top_down else 2
    palette = b'\x00\x00\x00\x00' + b'\x40\x80\xff\x00'  # black and RGB ff8040
    top = b'\x00\x01\x00\x00'
    bottom = b'\x01\x00\x01\x00'
    data = top + bottom if top_down else bottom + top
    offset = 14 + 40 + len(palette)
    return (struct.pack('<2sIHHI', b'BM', offset + len(data), 0, 0, offset) +
            struct.pack('<IiiHHIIiiII', 40, width, height, 1, 8, 0,
                        len(data), 0, 0, 2, 0) + palette + data)


def archive_fixture(members):
    directories = ['iff\\256\\', 'gui\\']
    extensions = ['.gui', '.lbm', '.pcx', '.bmp']
    file_body = bytearray(b'\0' * 4)
    entries = []
    for filename, data in members.items():
        path, basename = filename.rsplit('/', 1)
        name, ext = basename.rsplit('.', 1)
        encoded = (bytes([ord('0') + directories.index(path.replace('/', '\\') + '\\')]) +
                   name.encode('ascii') + bytes([ord('A') + extensions.index('.' + ext)]))
        entries.append((hash_path(encoded), len(file_body), len(data)))
        file_body.extend(data)
    offset = len(file_body)
    struct.pack_into('<I', file_body, 0, offset)
    names = b''.join(bytes([len(item)]) + item.encode('ascii')
                     for item in directories + extensions)
    file_body.extend(struct.pack('<4H', len(entries), len(directories),
                                 len(extensions), len(names)))
    file_body.extend(names)
    # The actual format indexes signed ascending 32-bit hashes.
    for entry in sorted(entries, key=lambda item: (item[0] ^ 0x80000000)):
        file_body.extend(struct.pack('<III', *entry))
    return bytes(file_body)


class DIDArchiveTests(unittest.TestCase):
    def test_provisional_raw_grid_families(self):
        pixels = bytes(range(256)) * 156 + bytes(range(64))
        self.assertEqual(len(pixels), GRID_BYTES)
        self.assertEqual(grid_candidate(pixels), ('bare-40000', pixels))
        self.assertEqual(grid_candidate(GRID_PREFIX + pixels),
                         ('prefix-01030064c8', pixels))
        self.assertIsNone(grid_candidate(b'other' + pixels))
        self.assertEqual(struct.unpack_from('>II', preview_png(pixels, width=100), 16),
                         (100, 400))
        with self.assertRaisesRegex(ValueError, 'divide'):
            preview_png(pixels, width=201)
        fixture = archive_fixture({'iff/256/a.lbm': pixels,
                                   'iff/256/b.lbm': GRID_PREFIX + pixels})
        families, _ = grid_audit(DIDArchive(fixture))
        self.assertEqual(dict(families), {'bare-40000': 1, 'prefix-01030064c8': 1})

    def test_guest_lookup_trace_resolution(self):
        archive = DIDArchive(archive_fixture({
            'iff/256/image.lbm': image_fixture(),
        }))
        self.assertEqual(parse_request('archive-lookup: archive=DID.DAT path=".\\\\IFF\\\\256\\\\image.lbm"'),
                         ('DID.DAT', '.\\IFF\\256\\image.lbm'))
        self.assertEqual(normalize_path('.\\IFF\\256\\image.lbm'),
                         'IFF\\256\\image.lbm')
        self.assertIn('found hash=', resolve_request(archive, '.\\IFF\\256\\image.lbm'))
        self.assertEqual(resolve_request(archive, '.\\IFF\\256\\absent.lbm'), 'missing')
        self.assertIsNone(parse_request('status=wall_time_limit'))

    def test_known_original_archive_hash_vectors(self):
        # Independent fixed guest resource indices from EF2000's two DATs.
        self.assertEqual(hash_path(b'AmainmenuA'), 0xac04eb66)
        self.assertEqual(hash_path(b'@mainmenuW'), 0xa110fd66)
        with self.assertRaisesRegex(ValueError, '15 bytes'):
            hash_path(b'abcdefghijklmnop')

    def test_index_lookup_and_bounds(self):
        fixture = archive_fixture({'gui/mainmenu.gui': b'window menu\r\n',
                                   'iff/256/image.lbm': image_fixture()})
        archive = DIDArchive(fixture)
        self.assertEqual(len(archive.entries), 2)
        self.assertEqual(archive.member(archive.lookup('GUI\\MAINMENU.GUI')),
                         b'window menu\r\n')
        self.assertEqual(archive.member(archive.lookup('iff/256/image.lbm'))[:4], b'FORM')
        self.assertIsNone(archive.lookup('gui/absent.gui'))
        with self.assertRaisesRegex(ValueError, 'extension'):
            archive.lookup('gui/mainmenu.xxx')
        with self.assertRaisesRegex(ValueError, 'length mismatch'):
            DIDArchive(fixture[:-1])
        bad = bytearray(fixture)
        struct.pack_into('<I', bad, 0, len(fixture) + 100)
        with self.assertRaisesRegex(ValueError, 'offset outside'):
            DIDArchive(bytes(bad))

    def test_ra_rejects_bad_headers(self):
        with self.assertRaisesRegex(ValueError, 'header'):
            decompress(b'bad')
        with self.assertRaisesRegex(ValueError, 'version'):
            decompress(b'RA\0\0' + b'\0' * 12)


class IFFTests(unittest.TestCase):
    def test_ilbm_byterun1_and_indexed_png(self):
        source = image_fixture()
        kind, sections = chunks(source)
        self.assertEqual(kind, b'ILBM')
        self.assertIn(b'CMAP', sections)
        bitmap = decode(source)
        self.assertEqual((bitmap.width, bitmap.height), (2, 1))
        self.assertEqual(bitmap.indexed, b'\0\1')
        self.assertIsNone(bitmap.alpha)
        png = encode_png(bitmap)
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(png[25], 3)  # palette PNG colour type
        compressed = png.index(b'IDAT')
        length = struct.unpack_from('>I', png, compressed - 4)[0]
        self.assertEqual(zlib.decompress(png[compressed + 4:compressed + 4 + length]),
                         b'\0\0\1')

    def test_transparency_pbm_raw_and_corrupt_rle(self):
        transparent = decode(image_fixture(transparent=0))
        self.assertEqual(transparent.alpha, b'\0\xff')
        self.assertEqual(encode_png(transparent)[25], 6)  # RGBA
        packed = decode(image_fixture(form=b'PBM ', pixels=(1, 0), compressed=False))
        self.assertEqual(packed.indexed, b'\1\0')
        with self.assertRaisesRegex(ValueError, 'truncated'):
            byterun_row(b'\x02\xaa', 0, 3)
        with self.assertRaisesRegex(ValueError, 'crossed'):
            byterun_row(b'\x81\xaa', 0, 3)
        with self.assertRaisesRegex(ValueError, 'FORM'):
            chunks(b'not an IFF image')


class BMPTests(unittest.TestCase):
    def test_indexed_palette_padding_and_orientation(self):
        for reverse in (False, True):
            source = bmp_fixture(top_down=reverse)
            self.assertTrue(is_bmp(source))
            bitmap = decode_bmp(source)
            self.assertEqual((bitmap.width, bitmap.height), (3, 2))
            self.assertEqual(bitmap.indexed, b'\x00\x01\x00\x01\x00\x01')
            self.assertEqual(bitmap.palette[3:6], b'\xff\x80\x40')
            self.assertEqual(encode_png(bitmap)[25], 3)
        with self.assertRaisesRegex(ValueError, 'truncated BMP pixels'):
            decode_bmp(bmp_fixture()[:-2])
        with self.assertRaisesRegex(ValueError, 'unsupported BMP planes'):
            source = bytearray(bmp_fixture())
            struct.pack_into('<H', source, 28, 24)
            decode_bmp(bytes(source))

    def test_all_images_includes_native_bmp_from_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / 'game'
            game.mkdir()
            (game / 'DID1ST.DAT').write_bytes(archive_fixture({
                'iff/256/controls.bmp': bmp_fixture()}))
            report = inventory(game)
            self.assertEqual(report['archives'][0]['recognized_pictures'], {'.bmp': 1})
            self.assertEqual(report['archives'][0]['unrecognized_members'], 0)
            manifest = extract(game, Path(directory) / 'out', [], all_images=True)
            self.assertEqual(len(manifest), 1)
            self.assertEqual(manifest[0]['kind'], 'image')
            self.assertEqual((manifest[0]['width'], manifest[0]['height']), (3, 2))
            expected = Path(directory) / 'out' / 'DID1ST' / (
                'hash_' + manifest[0]['hash'] + '.png')
            self.assertTrue(expected.is_file())


class OtherImageFormatTests(unittest.TestCase):
    def test_indexed_pcx_with_palette(self):
        head = bytearray(128)
        head[:4] = b'\x0a\x05\x01\x08'
        struct.pack_into('<4H', head, 4, 0, 0, 1, 0)
        head[65] = 1
        struct.pack_into('<H', head, 66, 2)
        pcx = bytes(head) + b'\xc2\x05' + b'\x0c' + bytes(range(256)) * 3
        self.assertTrue(is_pcx(pcx))
        image = decode_pcx(pcx)
        self.assertEqual((image.width, image.height, image.indexed), (2, 1, b'\x05\x05'))
        self.assertEqual(encode_png(image)[25], 3)
        with self.assertRaisesRegex(ValueError, 'invalid PCX run'):
            decode_pcx(bytes(head) + b'\xc2' + b'\x0c' + bytes(range(256)) * 3)

    def test_bmp_export_preserves_palette_colours_and_row_padding(self):
        bitmap = decode(image_fixture())
        bmp = encode_bmp(bitmap)
        self.assertEqual(bmp[:2], b'BM')
        self.assertEqual(struct.unpack_from('<I', bmp, 2)[0], len(bmp))
        self.assertEqual(struct.unpack_from('<iiHH', bmp, 18), (2, 1, 1, 24))
        self.assertEqual(bmp[54:], b'\0\0\0\x40\x80\xff\0\0')
        transparent = decode(image_fixture(transparent=0))
        self.assertEqual(encode_bmp(transparent)[54:], bmp[54:])
        # BMP stores rows bottom-up, with padding and BGR channel ordering.
        two_rows = Image(1, 2, b'\x01\x00', bitmap.palette, b'\xff\x00')
        self.assertEqual(encode_bmp(two_rows)[54:],
                         b'\x00\x00\x00\x00\x40\x80\xff\x00')
        with self.assertRaisesRegex(ValueError, 'source mask'):
            encode_bmp(Image(2, 1, b'\0\1', bitmap.palette, b'\0'))

    def test_indexed_sprite_catalog_preserves_16_bit_pixels(self):
        data = (b'\x02' + struct.pack('<2H', 5, 11) +
                struct.pack('<BBHH', 2, 1, 0, 107) +
                struct.pack('<BBHH', 2, 1, 65535, 3))
        result = decode_spr(data)
        self.assertEqual(len(result), 2)
        self.assertEqual((result[0].width, result[0].height, result[0].pixels),
                         (2, 1, (0, 107)))
        self.assertEqual(result[1].pixels, (65535, 3))
        with self.assertRaisesRegex(ValueError, 'offset outside'):
            decode_spr(data[:1] + b'\0\0' + data[3:])


class GUIInventoryTests(unittest.TestCase):
    def test_window_icon_order_and_multilingual_callbacks(self):
        source = (b'startup InitMainMenu\r\npicture newgui03\n'
                  b'window menu\n  xpos 2\nicon start\n  key\tret\n'
                  b'  routine gpwClickCampaign ; intentional comment\n'
                  b'  text german Hauptmenue\nwindow quit\nicon close\n  screen arcade\n')
        tree = inspect_script(source)
        self.assertEqual([item['value'] for item in tree['globals']],
                         ['InitMainMenu', 'newgui03'])
        self.assertEqual([window['name'] for window in tree['windows']], ['menu', 'quit'])
        self.assertEqual(tree['windows'][0]['icons'][0]['directives'][0]['value'], 'ret')
        self.assertEqual(tree['windows'][0]['icons'][0]['directives'][1]['value'],
                         'gpwClickCampaign')
        self.assertEqual(tree['windows'][1]['icons'][0]['directives'][0]['value'], 'arcade')

    def test_inventory_and_named_export_are_non_overwriting(self):
        script = b'startup InitMainMenu\npicture newgui03\nwindow menu\nicon start\nkey ret\n'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / 'game'
            game.mkdir()
            (game / 'DID1ST.DAT').write_bytes(archive_fixture({
                'gui/mainmenu.gui': script,
                'iff/256/image.lbm': image_fixture(),
            }))
            loose = game / 'SCRN0.LBM'
            loose.write_bytes(image_fixture())
            report = inventory(game)
            self.assertEqual(report['archives'][0]['members'], 2)
            self.assertEqual(report['archives'][0]['recognized_pictures'], {'.lbm': 1})
            self.assertEqual(report['archives'][0]['unrecognized_members'], 1)
            self.assertEqual(report['loose_lbm'], ['SCRN0.LBM'])
            target = root / 'out'
            found = extract(game, target, ['gui/mainmenu.gui', 'iff/256/image.lbm'], bmp=True)
            self.assertEqual(len(found), 2)
            self.assertTrue((target / 'DID1ST/iff/256/image.png').is_file())
            self.assertEqual((target / 'DID1ST/iff/256/image.bmp').read_bytes()[:2], b'BM')
            parsed = json.loads((target / 'DID1ST/gui/mainmenu.json').read_text())
            self.assertEqual(parsed['windows'][0]['icons'][0]['name'], 'start')
            with self.assertRaises(FileExistsError):
                extract(game, target, ['gui/mainmenu.gui'])
            everything = extract(game, root / 'all', [], all_images=True)
            self.assertEqual(len(everything), 2)
            self.assertTrue((root / 'all/LOOSE/SCRN0.png').is_file())
            self.assertEqual(gallery(root / 'all'), 2)
            html = (root / 'all/gallery.html').read_text(encoding='utf-8')
            self.assertIn('DID1ST/hash_', html)
            self.assertIn('LOOSE/SCRN0.png', html)
            self.assertIn('2 × 1', html)
            with self.assertRaises(FileExistsError):
                gallery(root / 'all')
        with self.assertRaisesRegex(ValueError, 'unsafe'):
            safe_relative('../escape.lbm')

    def test_gallery_validates_manifest_paths_and_missing_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = [{'kind': 'image', 'source': 'DID1ST.DAT',
                         'name': '../escape.lbm', 'width': 2, 'height': 1}]
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'unsafe'):
                gallery(root)
            manifest[0]['name'] = 'image.lbm'
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'missing extracted image'):
                gallery(root)
            manifest[0]['source'] = '../../foreign'
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'unsupported gallery source'):
                gallery(root)


@unittest.skipUnless(os.environ.get('EF2000_DATA_DIR'),
                     'set EF2000_DATA_DIR to run original DID archive and GUI replay checks')
class OriginalGUIAssetTests(unittest.TestCase):
    def test_original_indexed_bmp_missing_from_older_all_images_scan(self):
        archive = DIDArchive.from_path(Path(os.environ['EF2000_DATA_DIR']) / 'DID.DAT')
        original = archive.member(archive.by_hash[0x82cc4961])
        self.assertTrue(is_bmp(original))
        image = decode_bmp(original)
        self.assertEqual((image.width, image.height), (320, 200))
        self.assertEqual(len(image.indexed), 64_000)
        self.assertEqual(len(image.palette), 768)

    def test_original_gui_scripts_and_compressed_menu_images(self):
        game = Path(os.environ['EF2000_DATA_DIR'])
        first = DIDArchive.from_path(game / 'DID1ST.DAT')
        main = DIDArchive.from_path(game / 'DID.DAT')
        self.assertEqual((len(first.entries), len(main.entries)), (580, 8416))
        menu = first.member(first.lookup('gui/mainmenu.gui'))
        model = inspect_script(menu)
        self.assertEqual((len(model['windows']),
                          sum(len(w['icons']) for w in model['windows'])), (15, 58))
        self.assertEqual(model['globals'][1]['value'], 'newgui03')
        background = decode(first.member(first.lookup('iff/256/newgui03.lbm')))
        self.assertEqual((background.width, background.height), (640, 400))
        # Full options screens are useful original-style reference bitmaps,
        # although the menu's box/bkd directives imply runtime GUI drawing.
        screenshots = (0x94eb76bc, 0x9dac76bc, 0x950b6052)
        first_by_hash = {entry.hash: entry for entry in first.entries}
        for key in screenshots:
            bitmap = decode(first.member(first_by_hash[key]))
            self.assertEqual((bitmap.width, bitmap.height), (640, 400))
        compressed = main.lookup('iff/256/mainselc.lbm')
        self.assertTrue(main.raw_member(compressed).startswith(b'RA'))
        decoded = main.member(compressed)
        self.assertEqual(hashlib.sha256(decoded).hexdigest(),
                         '0112fcb9e1b41c5bc3aeed8d874d8444f192eca2f5c49908c004eeaf94a0be3f')
        image = decode(decoded)
        self.assertEqual((image.width, image.height), (320, 200))
        self.assertEqual(main.member_head(compressed), decoded[:12])
        sprites = decode_spr(main.member(main.lookup('spr/pitfont.spr')))
        self.assertEqual(len(sprites), 45)
        self.assertEqual({(item.width, item.height) for item in sprites}, {(4, 5)})


if __name__ == '__main__':
    unittest.main()
