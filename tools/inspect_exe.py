#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Read-only DOS executable triage. Offsets are file offsets, not addresses."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


def inspect(data):
    if len(data) < 28 or data[:2] != b'MZ':
        raise ValueError('expected an MZ executable with a complete base header')
    fields = struct.unpack_from('<13H', data, 2)
    names = ('last_page_bytes', 'pages', 'relocation_count', 'header_paragraphs',
             'min_extra_paragraphs', 'max_extra_paragraphs', 'ss', 'sp',
             'checksum', 'ip', 'cs', 'relocation_table_offset', 'overlay')
    header = dict(zip(names, fields))
    size = header['header_paragraphs'] * 16
    end = (header['pages'] - 1) * 512 + (header['last_page_bytes'] or 512)
    if size < 28 or size > len(data) or end < size or end > len(data):
        raise ValueError('MZ image bounds exceed file or contradict header')
    entry = size + header['cs'] * 16 + header['ip']
    if entry >= end:
        raise ValueError('MZ entry lies outside the declared DOS image')
    reloc_start = header['relocation_table_offset']
    reloc_end = reloc_start + header['relocation_count'] * 4
    if header['relocation_count'] and (reloc_start < 28 or reloc_end > size):
        raise ValueError('MZ relocation table lies outside header')
    relocations = []
    for offset in range(reloc_start, reloc_end, 4):
        off, seg = struct.unpack_from('<HH', data, offset)
        target = size + seg * 16 + off
        if target + 2 > end:
            raise ValueError('MZ relocation target lies outside DOS image')
        relocations.append({'segment': seg, 'offset': off, 'file_offset': target})
    markers = {}
    for marker in (b'B23X', b'WATCOM', b'DOS extender Copyright',
                   b'__X386_VM_DISABLED', b'RATIONAL DOS/4G'):
        pos = data.find(marker)
        if pos >= 0:
            markers[marker.decode('ascii')] = pos
    return {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'mz': header, 'dos_image_end': end, 'load_module_offset': size,
            'dos_entry_file_offset': entry, 'trailing_bytes': len(data) - end,
            'relocations': relocations, 'markers': markers,
            'note': 'Markers alone do not identify the protected-mode file format.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.executable.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'{args.executable}: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
