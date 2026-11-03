#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Decode the bounded, read-only Sound Blaster DSP reference probe."""

import argparse
import json
from pathlib import Path
import struct

FIELDS = ('reset_response', 'write_status', 'version_major', 'version_minor',
          'irq_setup', 'dma_setup', 'interrupt_status', 'empty_read_status')


def decode(data):
    if len(data) != 48 or data[:4] != b'SBDP':
        raise ValueError('expected a 48-byte SBDP record')
    if struct.unpack_from('<I', data, 4)[0] != 1 or data[8:12] != b'DONE':
        raise ValueError('unsupported or incomplete SBDP probe')
    values = dict(zip(FIELDS, struct.unpack_from('<8I', data, 16)))
    if any(value > 255 for value in values.values()):
        raise ValueError('SBDP byte sample outside its port width')
    return {'format': 'SBDP', 'version': 1, 'samples': values}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('record', type=Path)
    args = parser.parse_args()
    print(json.dumps(decode(args.record.read_bytes()), indent=2))


if __name__ == '__main__':
    main()
