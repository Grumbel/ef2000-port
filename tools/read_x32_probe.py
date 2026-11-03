#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Decode the record emitted by probes/x32_services.S."""

import argparse
import json
from pathlib import Path
import struct


def decode(data):
    if len(data) != 0x100 or data[:8] != b'X32R\x04\0\0\0' or data[0xfc:] != b'DONE':
        raise ValueError('invalid or incomplete X-32 reference record')
    def word(offset):
        return struct.unpack_from('<H', data, offset)[0]
    def dword(offset):
        return struct.unpack_from('<I', data, offset)[0]
    return {
        'entry': {
            'esp': dword(8),
            **{name: word(offset) for name, offset in
               [('cs', 0xc), ('ds', 0xe), ('es', 0x10), ('fs', 0x12), ('gs', 0x14), ('ss', 0x16)]},
            'eflags': dword(0x18),
            **{name: dword(offset) for name, offset in
               [('ds_limit', 0x1c), ('ss_limit', 0x20), ('fs_limit', 0x24),
                ('gs_limit', 0x28), ('ds_rights', 0x2c), ('zero_selector', 0x30),
                ('zero_base_offset', 0x34), ('auxiliary_selector', 0x38)]},
        },
        '3505': {name: dword(offset) for name, offset in
                 [('eflags', 0x3c), ('eax', 0x40), ('ecx', 0x44),
                  ('ds_limit', 0x48), ('ds_rights', 0x4c)]},
        '3509': {
            'input': {name: dword(offset) for name, offset in
                      [('ebx', 0x50), ('ecx', 0x54), ('esi', 0x58), ('eflags', 0xc0)]},
            'output': {name: dword(offset) for name, offset in
                       [('eflags', 0x5c), ('eax', 0x60), ('ebx', 0x64),
                        ('ecx', 0x68), ('edx', 0x6c), ('esi', 0x70)]},
        },
        'allocation_before': {name: dword(0x90 + i * 4) for i, name in enumerate(
            ['heap_end', 'stack_end', 'free', 'minimum_stack', 'linear_base', 'mode'])},
        'allocation_after': {name: dword(0xa8 + i * 4) for i, name in enumerate(
            ['heap_end', 'stack_end', 'free', 'minimum_stack', 'linear_base', 'mode'])},
        '3508': {name: dword(offset) for name, offset in
                 [('eax', 0xc4), ('ecx', 0xc8), ('eflags', 0xcc), ('heap_end', 0xd0),
                  ('stack_end', 0xd4), ('free', 0xd8), ('heap_ceiling', 0xe0), ('input_flags', 0xe4)]},
        'dos_version': {name: dword(offset) for name, offset in
                        [('eax', 0x74), ('ebx', 0x78), ('ecx', 0x7c), ('edx', 0x80),
                         ('esi', 0x84), ('edi', 0x88), ('eflags', 0x8c)]},
    }


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
