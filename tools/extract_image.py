#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Extract the fingerprinted EF2000 X-32 image; never execute it."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

try:
    from .inspect_exe import inspect
except ImportError:
    from inspect_exe import inspect


SUPPORTED_SHA256 = 'f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd'


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def decode_layout(data):
    """Decode this loader layout, not a general FlashTek format parser.

    The CLI additionally requires the known executable hash. Keeping structural
    decoding separate permits synthetic malformed-input tests without game data.
    """
    mz = inspect(data)
    module = mz['load_module_offset']
    if (module != 0x30 or data[0x28:0x2c] != b'B23X' or
            module + 0x14 > mz['dos_image_end']):
        raise ValueError('unsupported first-stage loader layout')
    source = u16(data, module)
    size = u16(data, module + 2)
    payload_offset = source + size
    data_offset = u16(data, module + 0x12) * 16
    code_offset = u16(data, module + 8) * 16
    if not (0 < source < payload_offset <= len(data)):
        raise ValueError('second-stage file range is invalid')
    if not (0x0c <= data_offset and data_offset + 0x0c <= size):
        raise ValueError('second-stage data header is outside loaded block')
    if not (data_offset <= code_offset < size):
        raise ValueError('second-stage entry is outside loaded block')
    stage2 = data[source:payload_offset]
    # _edata and _end are emitted by the Watcom X-32 startup module.
    initialized_size = u32(stage2, data_offset + 4)
    bss_end = u32(stage2, data_offset + 8)
    if initialized_size != len(data) - payload_offset:
        raise ValueError('initialized image size disagrees with file length')
    if not (0x86 < initialized_size <= bss_end <= 0xffffffff):
        raise ValueError('invalid initialized/BSS image boundaries')
    relocations = []
    for cursor in range(0, data_offset, 2):
        target = u16(stage2, cursor)
        if target == 0xffff:
            break
        if data_offset + target + 2 > size:
            raise ValueError('second-stage relocation target outside loaded block')
        relocations.append(target)
    else:
        raise ValueError('unterminated second-stage relocation table')
    return {
        'mz': mz,
        'stage2_file_offset': source,
        'stage2_size': size,
        'stage2_data_offset': data_offset,
        'stage2_entry_offset': code_offset,
        'stage2_relocation_offsets': relocations,
        'stage2_relocation_table_size': cursor + 2,
        'payload_file_offset': payload_offset,
        'initialized_size': initialized_size,
        'bss_end': bss_end,
        'protected_entry_offset': 0x86,
    }


def relocate_words(image, offsets, segment):
    """Apply the loader's 16-bit segment additions, including wraparound."""
    if not 0 <= segment <= 0xffff:
        raise ValueError('segment must fit in 16 bits')
    result = bytearray(image)
    for offset in offsets:
        if offset < 0 or offset + 2 > len(result):
            raise ValueError('relocation target outside image')
        struct.pack_into('<H', result, offset, (u16(result, offset) + segment) & 0xffff)
    return bytes(result)


def extract(data, load_segment=0x1000):
    """Produce images with static relocations only, not a runnable snapshot."""
    if hashlib.sha256(data).hexdigest() != SUPPORTED_SHA256:
        raise ValueError('unsupported executable SHA-256; no extraction attempted')
    layout = decode_layout(data)
    mz = layout['mz']
    module = mz['load_module_offset']
    stage2_data_segment = load_segment + layout['stage2_data_offset'] // 16
    # Keep the complete second-stage read below the VGA aperture in this model.
    if not (0 <= load_segment and
            load_segment * 16 + layout['stage2_size'] <= 0xa0000):
        raise ValueError('load segment must place the second-stage block below VGA memory')
    stage1 = relocate_words(
        data[module:mz['dos_image_end']],
        [reloc['file_offset'] - module for reloc in mz['relocations']],
        load_segment,
    )
    start = layout['stage2_file_offset']
    stage2 = relocate_words(
        data[start:start + layout['stage2_size']],
        [layout['stage2_data_offset'] + target
         for target in layout['stage2_relocation_offsets']],
        stage2_data_segment,
    )
    payload = data[layout['payload_file_offset']:]
    manifest = {
        'source_sha256': mz['sha256'],
        'format': 'EF2000 B23X / FlashTek X-32 (version-specific profile)',
        'layout': {key: value for key, value in layout.items() if key != 'mz'},
        'real_mode_model': {
            'load_segment': load_segment,
            'stage1_cs': load_segment + mz['mz']['cs'],
            'stage1_ip': mz['mz']['ip'],
            'stage1_ss': load_segment + mz['mz']['ss'],
            'stage1_sp': mz['mz']['sp'],
            'stage2_data_segment': stage2_data_segment,
            'stage2_code_segment': load_segment + layout['stage2_entry_offset'] // 16,
            'stage2_ip': 0,
            'stage2_relocations_applied': len(layout['stage2_relocation_offsets']),
        },
        'protected_image': {
            'address_space': 'offsets relative to the original 32-bit selectors',
            'initialized_range': [0, layout['initialized_size']],
            'startup_zero_fill_range': [layout['initialized_size'], layout['bss_end']],
            'entry_offset': layout['protected_entry_offset'],
            'entry_file_offset': layout['payload_file_offset'] + layout['protected_entry_offset'],
            'payload_sha256': hashlib.sha256(payload).hexdigest(),
            'runtime_patches_required': True,
        },
        'limitations': [
            'Static extraction only; no DOS, DPMI, selector, stack, or device setup.',
            'stage2-relocated.bin contains only the segment relocation pass; '
            'subsequent extender patches are not applied.',
            'payload.bin is initialized file bytes, not a runnable memory snapshot.',
            'Selector bases and initial ESP depend on runtime memory allocation.',
        ],
    }
    return manifest, {
        'stage1-relocated.bin': stage1,
        'stage2-relocated.bin': stage2,
        'payload.bin': payload,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True,
                        help='new output directory (existing paths are refused)')
    parser.add_argument('--load-segment', type=lambda value: int(value, 0), default=0x1000)
    args = parser.parse_args()
    try:
        manifest, images = extract(args.executable.read_bytes(), args.load_segment)
        args.output.mkdir(parents=True, exist_ok=False)
        for name, image in images.items():
            (args.output / name).write_bytes(image)
        (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    print(f'Extracted {len(images["payload.bin"]):,} initialized bytes to {args.output}')


if __name__ == '__main__':
    main()
