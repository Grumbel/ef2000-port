#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Export reproducible, address/byte annotated linear ASM windows under local/.

Tracker sizes are not verified function extents. This is a decoding inventory,
not a control-flow recovery pass or evidence that decoded bytes are code.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

try:
    from . import decomp_status, ghidra_ef2000
except ImportError:
    import decomp_status
    import ghidra_ef2000

LINE = re.compile(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)\s*(.*?)\s*$')
DIRECT = re.compile(r'^(?:0x)?([0-9a-f]+)(?:\s|$)')


def decode_window(image, address, size, *, objdump='objdump', runner=subprocess.run):
    result = runner([objdump, '-b', 'binary', '-m', 'i386', '-M', 'intel',
                     '-D', '-w', '--disassemble-zeroes',
                     f'--start-address={address:#x}',
                     f'--stop-address={address + size:#x}', str(image)],
                    check=True, capture_output=True, text=True)
    instructions = []
    for line in result.stdout.splitlines():
        match = LINE.match(line)
        if not match:
            continue
        offset = int(match[1], 16)
        encoded = bytes.fromhex(match[2])
        parts = match[3].split(None, 1)
        mnemonic = parts[0] if parts else ''
        operands = parts[1] if len(parts) > 1 else ''
        instruction = dict(address=offset, bytes=encoded.hex(),
                           mnemonic=mnemonic, operands=operands)
        if mnemonic == 'call' or mnemonic.startswith('j') or mnemonic.startswith('loop'):
            target = DIRECT.match(operands)
            if target:
                instruction['direct_target'] = int(target[1], 16)
        instructions.append(instruction)
    return instructions


def export_inventory(image, rows, output, *, objdump='objdump', runner=subprocess.run):
    """Caller supplies a validated payload. Never change tracker recovery states."""
    payload = image.read_bytes()
    prepared = []
    seen = set()
    for row in rows:
        address, size = int(row['addr'], 16), int(row['size'])
        if address in seen:
            raise ValueError(f'duplicate entry {address:#x}')
        if address < 0 or size <= 0 or address + size > len(payload):
            raise ValueError(f'invalid linear window at {address:#x}, size {size}')
        seen.add(address)
        prepared.append((address, size, row))
    # Refuse overwrite: stale files must not silently become part of a new run.
    output.mkdir(parents=True, exist_ok=False)
    version = runner([objdump, '--version'], check=True, capture_output=True,
                     text=True).stdout.splitlines()[0]
    entries = []
    for address, size, row in sorted(prepared):
        instructions = decode_window(image, address, size, objdump=objdump, runner=runner)
        cursor = address
        for insn in instructions:
            data = bytes.fromhex(insn['bytes'])
            if insn['address'] != cursor or not data or data != payload[cursor:cursor + len(data)]:
                raise ValueError(f'disassembly byte/continuity mismatch at {cursor:#x}')
            cursor += len(data)
        if cursor != address + size:
            raise ValueError(f'incomplete linear disassembly at {address:#x}')
        stem = f'{address:08x}'
        overlaps = [other for other, length, _ in prepared
                    if other != address and other < address + size and address < other + length]
        record = dict(address=address, size=size, name=row['name'],
                      boundary='unverified-linear-window',
                      overlapping_entries=sorted(overlaps),
                      undecodable_addresses=[i['address'] for i in instructions
                                             if i['mnemonic'] in ('(bad)', '.byte')],
                      instructions=instructions)
        (output / (stem + '.json')).write_text(json.dumps(record, indent=2) + '\n')
        listing = [f'; {address:#010x} {row["name"]}: unverified linear window ({size} bytes)']
        listing += [f'{i["address"]:08x}: {i["bytes"]:<30} {i["mnemonic"]} {i["operands"]}'.rstrip()
                    for i in instructions]
        (output / (stem + '.asm')).write_text('\n'.join(listing) + '\n')
        entries.append(dict(address=address, size=size, name=row['name'],
                            state=row['state'], kind=row['kind'], module=row['module'],
                            overlapping_entries=sorted(overlaps),
                            instruction_count=len(instructions), file=stem + '.json'))
    manifest = dict(schema=1, payload_sha256=hashlib.sha256(payload).hexdigest(),
                    decoder=version, boundary='unverified-linear-window',
                    overlap_scope='selected-tracker-entries',
                    selected_rows_sha256=hashlib.sha256(json.dumps(
                        [row for _, _, row in sorted(prepared)], sort_keys=True).encode()).hexdigest(),
                    warning='Decoded windows may include data and omit noncontiguous function ranges.',
                    entries=entries)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, default=decomp_status.DEFAULT_WORKSPACE)
    parser.add_argument('--tracker', type=Path, default=decomp_status.DEFAULT_TRACKER)
    parser.add_argument('--address', action='append', type=lambda value: int(value, 0))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--objdump', default='objdump')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ghidra_ef2000.ROOT / 'local'):
            raise ValueError('original ASM exports must stay under the ignored local/ directory')
        workspace = ghidra_ef2000.validated_workspace(args.workspace)
        rows = decomp_status.read_tracker(args.tracker)
        if args.address:
            requested = set(args.address)
            rows = [row for row in rows if int(row['addr'], 16) in requested]
            missing = requested - {int(row['addr'], 16) for row in rows}
            if missing:
                raise ValueError(f'addresses absent from tracker: {sorted(missing)}')
        manifest = export_inventory(workspace / ghidra_ef2000.IMAGE_NAME, rows,
                                    output, objdump=args.objdump)
        print(f'Exported {len(manifest["entries"])} linear windows to {output}')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_inventory: {error}\n')


if __name__ == '__main__':
    main()
