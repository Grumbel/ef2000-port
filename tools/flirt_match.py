# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Conservative OMF code-public fingerprints for EF2000 library identification.

Input objects are extracted OpenWatcom .obj modules, not a WLIB archive.
The generated fingerprints contain library code bytes: keep them in local/,
and review match proposals before modifying decomp/symbols/functions.tsv.
"""

import argparse
import json
from pathlib import Path
import sys


class OMFError(ValueError):
    pass


def field(data, cursor, size=1):
    if cursor + size > len(data):
        raise OMFError('truncated OMF field')
    return int.from_bytes(data[cursor:cursor + size], 'little'), cursor + size


def index(data, cursor):
    first, cursor = field(data, cursor)
    if first & 0x80:
        second, cursor = field(data, cursor)
        return ((first & 0x7f) << 8) | second, cursor
    return first, cursor


def name(data, cursor):
    length, cursor = field(data, cursor)
    if cursor + length > len(data):
        raise OMFError('truncated OMF name')
    return data[cursor:cursor + length].decode('latin-1'), cursor + length


def records(data):
    cursor = 0
    while cursor < len(data):
        kind, cursor = field(data, cursor)
        length, cursor = field(data, cursor, 2)
        if length == 0 or cursor + length > len(data):
            raise OMFError('bad OMF record length')
        payload = data[cursor:cursor + length - 1]
        checksum = data[cursor + length - 1]
        # OMF permits a zero checksum byte as a deliberate omission.
        if checksum and (kind + (length & 0xff) + (length >> 8) +
                         sum(payload) + checksum) & 0xff:
            raise OMFError('bad OMF record checksum')
        cursor += length
        yield kind, payload


WIDTHS = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2,
          9: 4, 11: 6, 13: 4}


def relocation_offsets(data, is32):
    """Decode FIXUPP threads and locations without resolving their targets."""
    cursor = 0
    offsets = []
    while cursor < len(data):
        first, cursor = field(data, cursor)
        if first & 0x80:
            second, cursor = field(data, cursor)
            location = (first >> 2) & 0xf
            if location not in WIDTHS:
                raise OMFError(f'unsupported fixup location {location}')
            offsets.append((((first & 3) << 8) | second, WIDTHS[location]))
            fixdat, cursor = field(data, cursor)
            if not fixdat & 0x80 and ((fixdat >> 4) & 7) <= 2:
                _, cursor = index(data, cursor)
            if not fixdat & 0x08:
                _, cursor = index(data, cursor)
            if not fixdat & 0x04:
                _, cursor = field(data, cursor, 4 if is32 else 2)
        else:
            # THREAD: bit 6 = FRAME, bits 4..2 = method, bits 1..0 = number.
            method = (first >> 2) & 7
            if method <= 2:
                _, cursor = index(data, cursor)
    return offsets


def code_publics(blob, source, prefix_size=64, minimum_fixed=16):
    """Return relocation-masked code-prefix facts from one OMF object module.

    Unsupported LIDATA/COMDAT or unrecognized relocation shapes are a hard
    error for that object. Never guess mask bytes or promote weak matches.
    """
    names = ['']
    segments = [None]
    publics = []
    last_data = None
    for kind, payload in records(blob):
        if kind == 0x96:  # LNAMES
            cursor = 0
            while cursor < len(payload):
                item, cursor = name(payload, cursor)
                names.append(item)
        elif kind in (0x98, 0x99):  # SEGDEF
            attr, cursor = field(payload, 0)
            if attr >> 5 == 0:  # absolute segment frame/offset
                _, cursor = field(payload, cursor, 3)
            size, cursor = field(payload, cursor, 4 if kind == 0x99 else 2)
            segname, cursor = index(payload, cursor)
            classname, cursor = index(payload, cursor)
            _, cursor = index(payload, cursor)
            if cursor != len(payload) or max(segname, classname) >= len(names):
                raise OMFError('invalid SEGDEF')
            if size > 4 * 1024 * 1024 or size == 0:
                raise OMFError('unsupported segment size')
            segments.append({'name': names[segname], 'class': names[classname],
                             'bytes': bytearray(size), 'written': bytearray(size),
                             'mask': bytearray(size), 'size': size})
        elif kind in (0x90, 0x91):  # PUBDEF
            cursor = 0
            _, cursor = index(payload, cursor)  # group
            segment, cursor = index(payload, cursor)
            if segment == 0:
                _, cursor = field(payload, cursor, 2)  # absolute frame
            while cursor < len(payload):
                symbol, cursor = name(payload, cursor)
                offset, cursor = field(payload, cursor, 4 if kind == 0x91 else 2)
                _, cursor = index(payload, cursor)  # type index
                publics.append((segment, offset, symbol))
        elif kind in (0xa0, 0xa1):  # LEDATA
            segment, cursor = index(payload, 0)
            offset, cursor = field(payload, cursor, 4 if kind == 0xa1 else 2)
            if segment == 0 or segment >= len(segments):
                raise OMFError('LEDATA outside defined segment')
            data = payload[cursor:]
            seg = segments[segment]
            if offset + len(data) > seg['size']:
                raise OMFError('LEDATA extends past segment')
            seg['bytes'][offset:offset + len(data)] = data
            seg['written'][offset:offset + len(data)] = b'\x01' * len(data)
            last_data = (segment, offset, len(data))
        elif kind in (0x9c, 0x9d):  # FIXUPP for preceding LEDATA
            if last_data is None:
                # Thread-only records can precede any data; fixups cannot.
                if any(width for _, width in relocation_offsets(payload, kind == 0x9d)):
                    raise OMFError('FIXUPP without LEDATA')
                continue
            segment, base, length = last_data
            for position, width in relocation_offsets(payload, kind == 0x9d):
                if position + width > length:
                    raise OMFError('fixup extends past LEDATA')
                segments[segment]['mask'][base + position:base + position + width] = (
                    b'\x01' * width)
        elif kind in (0xa2, 0xa3, 0xc2, 0xc3):
            # LIDATA/COMDAT need decompression or separate layout parsing.
            raise OMFError(f'unsupported OMF data record 0x{kind:02x}')
    signatures = []
    for segment, offset, symbol in publics:
        if not segment or segment >= len(segments):
            continue
        seg = segments[segment]
        if not (seg['class'].upper() == 'CODE' or
                seg['name'].upper() in ('_TEXT', '.TEXT', 'TEXT')):
            continue
        next_offset = min((at for idx, at, _ in publics
                           if idx == segment and at > offset),
                          default=seg['size'])
        length = min(prefix_size, next_offset - offset)
        if length < minimum_fixed or offset + length > seg['size']:
            continue
        if not all(seg['written'][offset:offset + length]):
            continue
        pattern = bytes(seg['bytes'][offset:offset + length])
        mask = bytes(seg['mask'][offset:offset + length])
        if len(pattern) - sum(mask) < minimum_fixed:
            continue
        signatures.append({'symbol': symbol, 'source': source,
                           'segment': seg['name'], 'function_extent': next_offset - offset,
                           'pattern': pattern.hex(), 'mask': mask.hex()})
    return signatures


def find_matches(image, tracker, signatures, minimum_fixed=16):
    matches = []
    for sig in signatures:
        pattern = bytes.fromhex(sig['pattern'])
        mask = bytes.fromhex(sig['mask'])
        if not pattern or len(pattern) != len(mask):
            raise ValueError('signature has invalid pattern/mask lengths')
        if len(pattern) - sum(mask) < minimum_fixed:
            continue
        for addr, size in tracker:
            if size < len(pattern) or addr + len(pattern) > len(image):
                continue
            candidate = image[addr:addr + len(pattern)]
            if all(a == b for a, b, masked in zip(candidate, pattern, mask)
                   if not masked):
                matches.append({'addr': f'0x{addr:08x}', 'symbol': sig['symbol'],
                                'source': sig['source'], 'fixed_bytes': len(pattern) - sum(mask),
                                'prefix_bytes': len(pattern)})
    # A name with multiple addresses, or an address with competing symbols,
    # is ambiguous. Return those for inspection; never auto-write the tracker.
    address_counts = {}
    symbol_counts = {}
    for match in matches:
        address_counts[match['addr']] = address_counts.get(match['addr'], 0) + 1
        symbol_counts[match['symbol']] = symbol_counts.get(match['symbol'], 0) + 1
    for match in matches:
        match['unique'] = (address_counts[match['addr']] == 1 and
                           symbol_counts[match['symbol']] == 1)
    return matches


def tracker_entries(path):
    import csv
    with path.open(newline='') as stream:
        rows = csv.DictReader(stream, delimiter='\t')
        return [(int(row['addr'], 16), int(row['size'])) for row in rows]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    build = sub.add_parser('build', help='fingerprint extracted OMF .obj modules')
    build.add_argument('objects', nargs='+', type=Path)
    build.add_argument('-o', '--output', type=Path, required=True)
    build.add_argument('--prefix', type=int, default=64)
    build.add_argument('--strict', action='store_true',
                       help='fail on unsupported objects instead of skipping them')
    match = sub.add_parser('match', help='review candidate image matches')
    match.add_argument('--signatures', type=Path, required=True)
    match.add_argument('--image', type=Path, required=True)
    match.add_argument('--functions', type=Path, required=True)
    match.add_argument('-o', '--output', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'build':
        if not 16 <= args.prefix <= 4096:
            parser.error('--prefix must be 16..4096')
        entries = []
        skipped = 0
        for obj in args.objects:
            if obj.is_dir():
                paths = sorted(p for p in obj.rglob('*')
                               if p.is_file() and p.suffix.lower() == '.obj')
            else:
                paths = [obj]
            for path in paths:
                try:
                    entries.extend(code_publics(path.read_bytes(),
                                                str(path.relative_to(obj)) if obj.is_dir()
                                                else path.name,
                                                prefix_size=args.prefix))
                except OMFError as error:
                    if args.strict:
                        raise SystemExit(f'{path}: {error}') from error
                    print(f'skip {path}: {error}', file=sys.stderr)
                    skipped += 1
        output = {'format': 'omf-code-prefix-v1', 'signatures': entries,
                  'skipped_objects': skipped}
    else:
        source = json.loads(args.signatures.read_text())
        if source.get('format') != 'omf-code-prefix-v1':
            parser.error('unknown fingerprint format')
        output = {'format': 'ef2000-library-match-v1',
                  'matches': find_matches(args.image.read_bytes(),
                                          tracker_entries(args.functions),
                                          source['signatures'])}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + '\n')
    print(f'{args.command}: {len(output.get("signatures", output.get("matches", [])))} entries'
          f' -> {args.output}')


if __name__ == '__main__':
    main()
