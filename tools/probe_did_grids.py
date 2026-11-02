#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Inspect unresolved 40,000-byte EF2000 DID members as 8-bit raster grids.

This is a *hypothesis-testing* tool: a useful grayscale preview does not
identify the member's original pathname, extension, palette, or semantics.
Nothing here is automatically incorporated into --all-images extraction.
"""

import argparse
from collections import Counter
from pathlib import Path

try:
    from .did_archive import DIDArchive
    from .ilbm_image import Image, encode_png
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import Image, encode_png


GRID_BYTES = 200 * 200
GRID_PREFIX = b'\x01\x03\x00\x64\xc8'
GRAY_PALETTE = bytes(value for level in range(256) for value in (level,) * 3)


def grid_candidate(blob):
    """Return (provisional family label, 40,000 raw bytes), or None."""
    if len(blob) == GRID_BYTES:
        return 'bare-40000', blob
    if len(blob) == GRID_BYTES + len(GRID_PREFIX) and blob.startswith(GRID_PREFIX):
        return 'prefix-01030064c8', blob[len(GRID_PREFIX):]
    return None


def preview_png(blob, width=200):
    candidate = grid_candidate(blob)
    if candidate is None:
        raise ValueError('member is not one of the two observed 40K grid families')
    if width <= 0 or width > GRID_BYTES or GRID_BYTES % width:
        raise ValueError('width must divide 40000')
    height = GRID_BYTES // width
    if height > 4096:
        raise ValueError('preview height exceeds 4096')
    return encode_png(Image(width, height, candidate[1], GRAY_PALETTE))


def audit(archive):
    families = Counter()
    candidates = []
    for entry in archive.entries:
        found = grid_candidate(archive.member(entry))
        if found:
            families[found[0]] += 1
            candidates.append((entry.hash, found[0]))
    return families, candidates


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path, help='original DID.DAT or DID1ST.DAT')
    parser.add_argument('--hash', type=lambda value: int(value, 16),
                        help='archive member hash to inspect (hex, with or without 0x)')
    parser.add_argument('--png', type=Path, help='write preview (requires --hash; never overwrites)')
    parser.add_argument('--width', type=int, default=200,
                        help='preview row width; default 200, must divide 40000')
    parser.add_argument('--list', action='store_true',
                        help='list each candidate hash, rather than counts only')
    args = parser.parse_args(argv)
    if args.png and args.hash is None:
        parser.error('--png requires --hash')
    archive = DIDArchive.from_path(args.archive)
    if args.hash is not None:
        entry = archive.by_hash.get(args.hash)
        if entry is None:
            parser.error(f'archive has no member hash {args.hash:08x}')
        blob = archive.member(entry)
        found = grid_candidate(blob)
        if not found:
            parser.error(f'member {args.hash:08x} is not an observed 40K grid family')
        print(f'{args.hash:08x}: {found[0]} payload={GRID_BYTES} bytes, '
              f'first16={found[1][:16].hex()}')
        if args.png:
            with args.png.open('xb') as output:
                output.write(preview_png(blob, args.width))
            print(f'provisional grayscale preview: {args.png} '
                  f'({args.width}x{GRID_BYTES // args.width})')
    else:
        families, candidates = audit(archive)
        for label, count in sorted(families.items()):
            print(f'{label}: {count} members')
        if args.list:
            for hashed, label in candidates:
                print(f'{hashed:08x} {label}')
        print('Unverified: members have no filename/extension in the archive index.')


if __name__ == '__main__':
    main()
