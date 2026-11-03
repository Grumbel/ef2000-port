#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Inventory and extract EF2000 GUI images and declarative menu scripts.

Accepts an original EF2000 installation directory (containing DID*.DAT).
All extracted game data stays in a caller-chosen output directory, never Git.
"""

import argparse
from collections import Counter
import html
import json
from pathlib import Path
from urllib.parse import quote

try:
    from .did_archive import DIDArchive
    from .ilbm_image import decode, encode_png, encode_bmp
    from .pcx_image import decode as decode_pcx, is_pcx
    from .bmp_image import decode as decode_bmp, is_bmp
    from .spr_image import decode as decode_spr
    from .menu_font import rip_font
    from .menu_symbols import rip_symbols
    from .gui_skin import rip_skin
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import decode, encode_png, encode_bmp
    from pcx_image import decode as decode_pcx, is_pcx
    from bmp_image import decode as decode_bmp, is_bmp
    from spr_image import decode as decode_spr
    from menu_font import rip_font
    from menu_symbols import rip_symbols
    from gui_skin import rip_skin


ARCHIVES = ('DID1ST.DAT', 'DID.DAT')


def inspect_script(data):
    """Lossless-in-order list of active directives, grouped by window/icon.

    This is an inventory, not an implementation of the original GUI parser:
    unknown directives are preserved; runtime conditions are not evaluated.
    """
    document = {'globals': [], 'windows': []}
    current = document['globals']
    for line_no, original in enumerate(data.decode('latin-1').splitlines(), 1):
        # The original menu language uses semicolons for comments.
        value = original.split(';', 1)[0].strip()
        if not value:
            continue
        words = value.split(None, 1)
        command = words[0].lower()
        args = words[1].strip() if len(words) == 2 else ''
        if command == 'window':
            window = {'name': args, 'line': line_no, 'directives': [], 'icons': []}
            document['windows'].append(window)
            current = window['directives']
        elif command == 'icon':
            if not document['windows']:
                current = document['globals']
            else:
                icon = {'name': args, 'line': line_no, 'directives': []}
                document['windows'][-1]['icons'].append(icon)
                current = icon['directives']
        else:
            current.append({'line': line_no, 'command': command, 'value': args})
    return document


def write_new(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as output:
        output.write(data)


def archives_from(directory):
    archives = []
    for name in ARCHIVES:
        path = directory / name
        if path.is_file():
            archives.append((name, DIDArchive.from_path(path)))
    if not archives:
        raise ValueError('no DID1ST.DAT or DID.DAT in game directory')
    return archives


def format_kind(blob):
    if blob.startswith(b'RA'):
        return 'RA'
    if blob.startswith(b'FORM') and blob[8:12] in (b'ILBM', b'PBM '):
        return blob[8:12].decode('ascii').strip()
    if is_pcx(blob):
        return 'PCX'
    if is_bmp(blob):
        return 'BMP'
    return 'other'


def image_suffix(archive, entry):
    """Detect supported pictures by decoded signature, not hidden hash/extension."""
    head = archive.member_head(entry)
    if head.startswith(b'FORM') and head[8:12] in (b'ILBM', b'PBM '):
        return '.lbm'
    if (head[:1] == b'\x0a' and head[2:4] == b'\x01\x08' and
            is_pcx(archive.member(entry))):
        return '.pcx'
    if head.startswith(b'BM') and is_bmp(archive.member(entry)):
        return '.bmp'
    return None


def inventory(game_dir):
    report = {'archives': [], 'loose_lbm': []}
    for filename, archive in archives_from(game_dir):
        kinds = Counter(format_kind(archive.raw_member(entry)) for entry in archive.entries)
        images = Counter(suffix for entry in archive.entries
                         if (suffix := image_suffix(archive, entry)))
        report['archives'].append({
            'file': filename,
            'members': len(archive.entries),
            'directories': list(archive.directories),
            'extensions': list(archive.extensions),
            'stored_types': dict(sorted(kinds.items())),
            'recognized_pictures': dict(sorted(images.items())),
            'unrecognized_members': len(archive.entries) - sum(images.values()),
        })
    for path in sorted(game_dir.rglob('*')):
        if path.is_file() and path.suffix.lower() == '.lbm':
            report['loose_lbm'].append(path.relative_to(game_dir).as_posix())
    return report


def safe_relative(name):
    relative = Path(name.replace('\\', '/'))
    if relative.is_absolute() or not relative.parts or '..' in relative.parts:
        raise ValueError(f'unsafe asset pathname: {name}')
    return relative


def extract(game_dir, output_dir, paths, all_images=False, bmp=False):
    if not paths and not all_images:
        raise ValueError('provide one or more guest paths or --all-images')
    manifest = []
    for archive_name, archive in archives_from(game_dir):
        selected = []
        for path in paths:
            relative = safe_relative(path.lower())
            try:
                entry = archive.lookup(path)
            except ValueError:
                # A resource name may be valid in only one of the two DAT
                # revisions (e.g. .spr in DID.DAT, not DID1ST.DAT).
                continue
            if entry:
                selected.append((relative, entry))
        if all_images:
            for entry in archive.entries:
                # Only decompress full RA payloads after identifying their
                # leading image signature; thousands of 3D members are skipped.
                if suffix := image_suffix(archive, entry):
                    selected.append((Path(f'hash_{entry.hash:08x}{suffix}'), entry))
        for relative, entry in selected:
            content = archive.member(entry)
            base = output_dir / archive_name.removesuffix('.DAT') / relative
            info = {'source': archive_name, 'name': relative.as_posix(),
                    'hash': f'{entry.hash:08x}', 'stored_size': entry.size,
                    'decoded_size': len(content)}
            if content.startswith(b'FORM') and content[8:12] in (b'ILBM', b'PBM '):
                image = decode(content)
                write_new(base.with_suffix('.png'), encode_png(image))
                if bmp:
                    write_new(base.with_suffix('.bmp'), encode_bmp(image))
                info.update({'kind': 'image', 'width': image.width,
                             'height': image.height, 'palette_colors': len(image.palette) // 3})
            elif is_pcx(content):
                image = decode_pcx(content)
                write_new(base.with_suffix('.png'), encode_png(image))
                if bmp:
                    write_new(base.with_suffix('.bmp'), encode_bmp(image))
                info.update({'kind': 'image', 'width': image.width,
                             'height': image.height, 'palette_colors': 256})
            elif is_bmp(content):
                image = decode_bmp(content)
                write_new(base.with_suffix('.png'), encode_png(image))
                if bmp:
                    write_new(base.with_suffix('.bmp'), encode_bmp(image))
                info.update({'kind': 'image', 'width': image.width,
                             'height': image.height, 'palette_colors': 256})
            elif relative.suffix.lower() == '.gui':
                write_new(base, content)
                tree = inspect_script(content)
                write_new(base.with_suffix('.json'),
                          (json.dumps(tree, indent=2, ensure_ascii=False) + '\n').encode('utf-8'))
                info.update({'kind': 'gui-script', 'windows': len(tree['windows']),
                             'icons': sum(len(w['icons']) for w in tree['windows'])})
            elif relative.suffix.lower() == '.spr':
                sprites = decode_spr(content)
                write_new(base, content)
                catalog = [{'width': item.width, 'height': item.height,
                            'pixels': list(item.pixels)} for item in sprites]
                write_new(base.with_suffix('.json'),
                          (json.dumps(catalog, indent=2) + '\n').encode('utf-8'))
                info.update({'kind': 'sprite-index', 'sprites': len(sprites)})
            else:
                write_new(base, content)
                info['kind'] = 'binary'
            manifest.append(info)
    if all_images:
        for path in sorted(game_dir.rglob('*')):
            if not path.is_file() or path.suffix.lower() != '.lbm':
                continue
            image = decode(path.read_bytes())
            relative = path.relative_to(game_dir)
            write_new((output_dir / 'LOOSE' / relative).with_suffix('.png'), encode_png(image))
            if bmp:
                write_new((output_dir / 'LOOSE' / relative).with_suffix('.bmp'), encode_bmp(image))
            manifest.append({'source': 'LOOSE', 'name': relative.as_posix(),
                             'kind': 'image', 'width': image.width, 'height': image.height})
    if not manifest:
        raise ValueError('no requested assets found in supplied archives')
    write_new(output_dir / 'manifest.json',
              (json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
    return manifest


def gallery(output_dir):
    """Build an offline, original-asset-local contact gallery from an extraction.

    A browser can display hundreds of hashed members without installing an
    image viewer or any Python graphics dependencies. All links stay inside
    the extraction directory; the gallery never embeds proprietary pixels.
    """
    manifest = json.loads((output_dir / 'manifest.json').read_text(encoding='utf-8'))
    images = []
    for entry in manifest:
        if entry.get('kind') != 'image':
            continue
        source = entry['source']
        if source not in ('DID1ST.DAT', 'DID.DAT', 'LOOSE'):
            raise ValueError(f'unsupported gallery source: {source}')
        name = safe_relative(entry['name'])
        relative = (Path(source.removesuffix('.DAT')) / name).with_suffix('.png')
        if not (output_dir / relative).is_file():
            raise ValueError(f'missing extracted image: {relative}')
        images.append((entry, relative))
    if not images:
        raise ValueError('manifest contains no extracted images')

    # Small images are the likeliest window controls or button art; lead with
    # those while retaining full-size backgrounds later in the same index.
    images.sort(key=lambda item: (item[0]['width'] * item[0]['height'],
                                  item[0]['source'], item[0]['name']))
    tiles = []
    for entry, path in images:
        name = html.escape(entry['name'], quote=True)
        source = html.escape(entry['source'], quote=True)
        url = quote(path.as_posix(), safe='/')
        dimensions = f"{entry['width']} × {entry['height']}"
        tiles.append(
            f'<figure><a href="{url}"><img src="{url}" loading="lazy" '
            f'alt="{name}" width="256" height="160"></a>'
            f'<figcaption><b>{name}</b><small>{source} · {dimensions}'
            f'</small></figcaption></figure>')
    markup = ('''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EF2000 extracted art gallery</title>
<style>
:root { color-scheme: dark; font: 14px system-ui, sans-serif; background: #152031; color: #dbe3ec; }
body { margin: 24px; } h1 { margin-bottom: 0; } p { color: #afbed1; }
main { display: grid; grid-template-columns: repeat(auto-fill,minmax(220px,1fr)); gap: 14px; }
figure { margin: 0; background: #25364a; border: 1px solid #61768f; min-width: 0; }
a { display: flex; height: 176px; padding: 8px; align-items: center; justify-content: center; }
img { max-width: 100%; max-height: 100%; width: auto; height: auto; object-fit: contain; image-rendering: pixelated; }
figcaption { padding: 9px; overflow-wrap: anywhere; } small { display: block; color: #bacadd; margin-top: 4px; }
</style></head><body><h1>EF2000 original image inventory</h1>
<p>Smallest first · Select a thumbnail to see its full-size PNG · Local files only.</p>
<main>''' + '\n'.join(tiles) + '</main></body></html>\n')
    write_new(output_dir / 'gallery.html', markup.encode('utf-8'))
    return len(images)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest='action', required=True)
    scan = modes.add_parser('inventory', help='show archive directories, types and loose images')
    scan.add_argument('game_dir', type=Path)
    scan.add_argument('--json', type=Path, help='write asset-free inventory JSON')
    unpack = modes.add_parser('extract', help='export original artwork and GUI definitions')
    unpack.add_argument('game_dir', type=Path)
    unpack.add_argument('paths', nargs='*', help='e.g. gui/mainmenu.gui iff/256/newgui03.lbm')
    unpack.add_argument('--out', type=Path, required=True, help='new output directory (no overwrite)')
    unpack.add_argument('--all-images', action='store_true', help='recover IFF/PCX/BMP images, named by hash')
    unpack.add_argument('--bmp', action='store_true', help='also export 24-bit BMP for SDL3 host overlays')
    unpack.add_argument('--gallery', action='store_true', help='also generate a standalone offline image gallery')
    display = modes.add_parser('gallery', help='make an offline HTML contact gallery of extracted PNGs')
    display.add_argument('extraction_dir', type=Path, help='directory containing manifest.json')
    font = modes.add_parser('font', help='recover proportional glyphs from original DID1ST font specimen')
    font.add_argument('game_dir', type=Path, help='original game installation with DID1ST.DAT')
    font.add_argument('--out', type=Path, required=True, help='new local font output directory')
    symbols = modes.add_parser('symbols', help='extract two-row symbol strip from original DID1ST specimen')
    symbols.add_argument('game_dir', type=Path, help='original game installation with DID1ST.DAT')
    symbols.add_argument('--out', type=Path, required=True, help='new local symbol output directory')
    skin = modes.add_parser('skin', help='export original full-screen GUI textures and tactical icon sheet')
    skin.add_argument('game_dir', type=Path, help='original installation with DID1ST.DAT')
    skin.add_argument('--out', type=Path, required=True, help='new local skin output directory')
    args = parser.parse_args(argv)
    try:
        if args.action == 'inventory':
            report = inventory(args.game_dir)
            print(json.dumps(report, indent=2))
            if args.json:
                write_new(args.json, (json.dumps(report, indent=2) + '\n').encode('utf-8'))
        elif args.action == 'gallery':
            count = gallery(args.extraction_dir)
            print(f'indexed {count} images in {args.extraction_dir / "gallery.html"}')
        elif args.action == 'font':
            glyphs, conflicts = rip_font(args.game_dir, args.out)
            print(f'recovered {len(glyphs)} menu glyphs into {args.out}')
            if conflicts:
                print(f'  non-identical repeated samples: {", ".join(conflicts)}')
        elif args.action == 'symbols':
            metadata = rip_symbols(args.game_dir, args.out)
            print(f'extracted {len(metadata["cells"])} specimen symbols into {args.out}')
        elif args.action == 'skin':
            metadata = rip_skin(args.game_dir, args.out)
            glyphs, _ = rip_font(args.game_dir, args.out)
            symbols = rip_symbols(args.game_dir, args.out)
            print(f'extracted {len(metadata["textures"])} original GUI textures and '
                  f'{len(glyphs)} menu glyphs and {len(symbols["cells"])} '
                  f'specimen symbols into {args.out}')
        else:
            result = extract(args.game_dir, args.out, args.paths, args.all_images, args.bmp)
            print(f'extracted {len(result)} GUI assets into {args.out}')
            if args.gallery:
                print(f'indexed {gallery(args.out)} images in {args.out / "gallery.html"}')
            if args.all_images:
                for name, count in sorted(Counter(item['source'] for item in result).items()):
                    print(f'  {name}: {count} images/resources')
            else:
                for entry in result:
                    print(f"  {entry['source']}: {entry['name']} -> {entry['kind']}")
    except (ValueError, OSError) as error:
        parser.exit(1, f'ef2000_gui_assets: {error}\n')


if __name__ == '__main__':
    main()
