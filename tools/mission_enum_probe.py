#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Recover EF2000 mission token-to-enum tables from the pinned X-32 payload.

Read-only: the user supplies the extracted original payload. No game text is
checked in; each index is the integer returned by its original reader.
"""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct


IMAGE_SIZE = 0x195fb0
IMAGE_SHA256 = '3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb'
MAX_STRING_BYTES = 256


@dataclass(frozen=True)
class Table:
    name: str
    base: int
    count: int
    reader: int


# These pointer-table addresses and loop bounds come from disassembly of
# the specified guest readers (MOV EDX,[EBX+base] and CMP EBX,count*4).
# Index order is authoritative for this one SHA-pinned payload only.
TABLES = (
    Table('allegiances', 0x132f5c, 11, 0x0000e9bc),
    Table('mission_types', 0x132e6c, 30, 0x0000eaf8),
    Table('plane_types', 0x132d34, 39, 0x0000eb48),
    Table('ship_types', 0x1333f8, 18, 0x0000eb9c),
    Table('ground_types', 0x133488, 27, 0x0000ebec),
    Table('flight_roles', 0x133304, 13, 0x0000ec3c),
    Table('target_types', 0x132fb4, 106, 0x0000ec8c),
    Table('statuses', 0x13336c, 10, 0x0000ece0),
    Table('waypoint_actions', 0x132c2c, 19, 0x0000ed30),
    Table('attack_node_actions', 0x132cc4, 3, 0x0000ef98),
    Table('weapons', 0x1335a0, 37, 0x0000eea0),
)

# 0x0000ea0c tests two sets of literal spellings, returning the same
# category integer for an abbreviated name and its expanded alias.
MOBILE_CLASS_ALIASES = (
    (0, 0x180c84), (1, 0x180c88), (2, 0x180c8c), (3, 0x180c90),
    (0, 0x180c94), (1, 0x180c9c), (2, 0x180ca4), (3, 0x180cac),
)


def read_c_string(payload, address):
    if not 0 <= address < len(payload):
        raise ValueError(f'string address 0x{address:08x} outside the payload')
    end = payload.find(b'\0', address, min(len(payload), address + MAX_STRING_BYTES))
    if end < 0:
        raise ValueError(f'unterminated string at guest address 0x{address:08x}')
    return payload[address:end].decode('latin-1')


def decode_table(payload, table):
    if table.base < 0 or table.count < 0 or table.base + table.count * 4 > len(payload):
        raise ValueError(f'pointer table {table.name} outside the payload')
    values = []
    for index in range(table.count):
        address, = struct.unpack_from('<I', payload, table.base + index * 4)
        values.append({'index': index, 'label': read_c_string(payload, address)})
    return {'guest_reader': f'0x{table.reader:08x}',
            'pointer_table': f'0x{table.base:08x}', 'entries': values}


def decode_aliases(payload):
    return {
        'guest_reader': '0x0000ea0c',
        'entries': [{'index': index, 'label': read_c_string(payload, address)}
                    for index, address in MOBILE_CLASS_ALIASES],
    }


def inspect_payload(payload, selected=None, verify=True):
    if verify and (len(payload) != IMAGE_SIZE or
                   hashlib.sha256(payload).hexdigest() != IMAGE_SHA256):
        raise ValueError('payload is not the SHA-pinned German EF2000 X-32 image')
    selected = selected or [table.name for table in TABLES] + ['mobile_class_aliases']
    found = {}
    for table in TABLES:
        if table.name in selected:
            found[table.name] = decode_table(payload, table)
    if 'mobile_class_aliases' in selected:
        found['mobile_class_aliases'] = decode_aliases(payload)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('payload', type=Path,
                        help='payload.bin extracted by tools/ghidra_ef2000.py import')
    parser.add_argument('--table', choices=[table.name for table in TABLES] +
                        ['mobile_class_aliases'], help='display one table and its indexed names')
    parser.add_argument('--json', action='store_true', help='export all requested tables as JSON')
    args = parser.parse_args()
    try:
        if args.payload.stat().st_size != IMAGE_SIZE:
            raise ValueError('unexpected payload size')
        payload = args.payload.read_bytes()
        result = inspect_payload(payload, selected=[args.table] if args.table else None)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        for name, table in result.items():
            print(f'{name}: {len(table["entries"])} labels, reader {table["guest_reader"]}')
            if args.table:
                for item in table['entries']:
                    print(f'  {item["index"]:3}: {item["label"]}')


if __name__ == '__main__':
    main()
