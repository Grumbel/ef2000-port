#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Decode the four DOS time samples emitted by probes/dos_time.S."""

import argparse
import json
from pathlib import Path
import struct


def decode(data):
    if len(data) != 0x94 or data[:8] != b'TIME\x01\0\0\0' or data[-4:] != b'DONE':
        raise ValueError('invalid or incomplete DOS time record')
    names = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'eflags', 'input_flags')
    return [dict(zip(names, struct.unpack_from('<8I', data, 16 + i * 32)))
            for i in range(4)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('record', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(decode(args.record.read_bytes()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
