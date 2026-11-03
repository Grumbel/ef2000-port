#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Decode DOS file/memory and X-32 vector/residency reference records."""

import argparse
import json
from pathlib import Path
import struct


def decode(data):
    profiles = {b'DMEM': (1, 2, 8), b'DFIL': (1, 7, 8), b'XVEC': (1, 10, 10)}
    profile = profiles.get(bytes(data[:4]))
    if profile is None:
        raise ValueError('unknown runtime reference record')
    version, count, width = profile
    if (len(data) != 20 + count * width * 4 or data[-4:] != b'DONE' or
            (struct.unpack_from('<H', data, 6)[0] if data[:4] == b'XVEC' else
             struct.unpack_from('<I', data, 4)[0]) != version):
        raise ValueError('invalid or incomplete runtime reference record')
    names = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'eflags', 'input_flags', 'es', 'ds')
    result = {'format': data[:4].decode(), 'version': version,
              'samples': [dict(zip(names, struct.unpack_from(f'<{width}I', data, 16 + i * width * 4)))
                          for i in range(count)]}
    if data[:4] == b'XVEC':
        result['saved_pm_selector'] = struct.unpack_from('<H', data, 4)[0]
        result['saved_rm_vector'], result['saved_pm_offset'] = struct.unpack_from('<II', data, 8)
    return result


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
