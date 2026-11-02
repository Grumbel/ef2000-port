#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Inventory and extract EF2000 GUI images and declarative menu scripts.

Accepts an original EF2000 installation directory (containing DID*.DAT).
All extracted game data stays in a caller-chosen output directory, never Git.
"""

import argparse
from collections import Counter
import json
from pathlib import Path

try:
    from .did_archive import DIDArchive
    from .ilbm_image import decode, encode_png, encode_bmp
    from .pcx_image import decode as decode_pcx, is_pcx
    from .spr_image import decode as decode_spr
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import decode, encode_png, encode_bmp
    from pcx_image import decode as decode_pcx, is_pcx
    from spr_image import decode as decode_spr


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
    return 'other'


def inventory(game_dir):
    report = {'archives': [], 'loose_lbm': []}
    for filename, archive in archives_from(game_dir):
        kinds = Counter(format_kind(archive.raw_member(entry)) for entry in archive.entries)
        report['archives'].append({
            'file': filename,
            'members': len(archive.entries),
            'directories': list(archive.directories),
            'extensions': list(archive.extensions),
            'stored_types': dict(sorted(kinds.items())),
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
                head = archive.member_head(entry)
                # Only decompress full RA payloads after identifying their
                # leading image signature; thousands of 3D members are skipped.
                if head.startswith(b'FORM') and head[8:12] in (b'ILBM', b'PBM '):
                    selected.append((Path(f'hash_{entry.hash:08x}.lbm'), entry))
                elif (head[:1] == b'\x0a' and head[2:4] == b'\x01\x08'
                      and is_pcx(archive.member(entry))):
                    selected.append((Path(f'hash_{entry.hash:08x}.pcx'), entry))
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
    unpack.add_argument('--all-images', action='store_true', help='recover every archive ILBM/PBM, named by hash')
    unpack.add_argument('--bmp', action='store_true', help='also export 24-bit BMP for SDL3 host overlays')
    args = parser.parse_args(argv)
    try:
        if args.action == 'inventory':
            report = inventory(args.game_dir)
            print(json.dumps(report, indent=2))
            if args.json:
                write_new(args.json, (json.dumps(report, indent=2) + '\n').encode('utf-8'))
        else:
            result = extract(args.game_dir, args.out, args.paths, args.all_images, args.bmp)
            print(f'extracted {len(result)} GUI assets into {args.out}')
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
