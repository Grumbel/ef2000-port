#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Export reproducible ASM windows or Ghidra body ranges under local/.

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


def control_flow_report(instructions):
    """Classify static edges without claiming that linear bytes are code.

    A target inside a decoded instruction is a blocker, not a new entry.
    Calls retain their fallthrough; unconditional jumps and returns do not.
    Indirect destinations and interrupt/system transitions require review.
    """
    starts = {i['address'] for i in instructions}
    edges = []
    for insn in instructions:
        site = insn['address']
        mnemonic = insn['mnemonic']
        next_address = site + len(bytes.fromhex(insn['bytes']))
        call = mnemonic in ('call', 'lcall')
        jump = mnemonic.startswith('j') or mnemonic.startswith('loop') or mnemonic == 'ljmp'
        unconditional = mnemonic in ('jmp', 'ljmp')
        terminal = mnemonic.startswith(('ret', 'iret')) or mnemonic in ('hlt', 'ud2')
        transition = mnemonic in ('int', 'int3', 'into', 'syscall', 'sysenter', 'sysexit', 'sysret')
        if call or jump:
            target = insn.get('direct_target')
            status = 'indirect-unresolved' if target is None else (
                'decoded-instruction' if target in starts else 'outside-decoded-starts')
            edges.append(dict(site=site, kind='call' if call else 'jump',
                              target=target, status=status))
        if transition or mnemonic in ('(bad)', '.byte'):
            edges.append(dict(site=site, kind='review', target=None,
                              status='system-transition' if transition else 'undecodable'))
        if not unconditional and not terminal:
            edges.append(dict(site=site, kind='fallthrough', target=next_address,
                              status='decoded-instruction' if next_address in starts
                              else 'outside-decoded-starts'))
    return dict(edges=edges, review_edges=[e for e in edges
                                          if e['status'] != 'decoded-instruction'])


def load_body_ranges(path, payload):
    """Read half-open Ghidra body spans, bound to the exact payload digest."""
    source = path.read_bytes()
    document = json.loads(source)
    if (document.get('schema') != 1 or
            document.get('boundary') != 'ghidra-body-ranges' or
            document.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('invalid or mismatched body-range provenance')
    bodies = {}
    if not isinstance(document.get('entries'), list):
        raise ValueError('invalid body-range entries')
    for entry in document['entries']:
        address, ranges = entry['address'], entry['ranges']
        if type(address) is not int or address in bodies or not ranges:
            raise ValueError('duplicate or invalid body-range entry')
        previous_end = -1
        for span in ranges:
            if (len(span) != 2 or any(type(v) is not int for v in span) or
                    not 0 <= span[0] < span[1] <= len(payload) or span[0] < previous_end):
                raise ValueError(f'invalid body ranges at {address:#x}')
            previous_end = span[1]
        if not any(start <= address < end for start, end in ranges):
            raise ValueError(f'entry outside body ranges at {address:#x}')
        layout = entry.get('instruction_spans')
        if layout is not None:
            previous_end = -1
            for site, length in layout:
                if (type(site) is not int or type(length) is not int or length <= 0 or
                        site < previous_end or
                        not any(start <= site and site + length <= end for start, end in ranges)):
                    raise ValueError(f'invalid instruction spans at {address:#x}')
                previous_end = site + length
        bodies[address] = entry
    return bodies, hashlib.sha256(source).hexdigest()


def export_inventory(image, rows, output, *, objdump='objdump', runner=subprocess.run,
                     body_ranges=None):
    """Caller supplies a validated payload. Never change tracker recovery states."""
    payload = image.read_bytes()
    bodies, body_digest = load_body_ranges(body_ranges, payload) if body_ranges else ({}, None)
    boundary = 'ghidra-body-ranges' if body_ranges else 'unverified-linear-window'
    prepared = []
    seen = set()
    for row in rows:
        address, size = int(row['addr'], 16), int(row['size'])
        if address in seen:
            raise ValueError(f'duplicate entry {address:#x}')
        if not body_ranges and (address < 0 or size <= 0 or address + size > len(payload)):
            raise ValueError(f'invalid linear window at {address:#x}, size {size}')
        if body_ranges and address not in bodies:
            raise ValueError(f'no Ghidra body ranges for {address:#x}')
        ranges = bodies[address]['ranges'] if body_ranges else [[address, address + size]]
        seen.add(address)
        prepared.append((address, ranges, row))
    # Refuse overwrite: stale files must not silently become part of a new run.
    output.mkdir(parents=True, exist_ok=False)
    version = runner([objdump, '--version'], check=True, capture_output=True,
                     text=True).stdout.splitlines()[0]
    entries = []
    for address, ranges, row in sorted(prepared):
        instructions = []
        for start, end in ranges:
            decoded = decode_window(image, start, end - start, objdump=objdump, runner=runner)
            cursor = start
            for insn in decoded:
                data = bytes.fromhex(insn['bytes'])
                if (insn['address'] != cursor or not data or cursor + len(data) > end or
                        data != payload[cursor:cursor + len(data)]):
                    raise ValueError(f'disassembly byte/continuity mismatch at {cursor:#x}')
                cursor += len(data)
            if cursor != end:
                raise ValueError(f'incomplete linear disassembly at {start:#x}')
            instructions.extend(decoded)
        size = sum(end - start for start, end in ranges)
        stem = f'{address:08x}'
        overlaps = [other for other, spans, _ in prepared
                    if other != address and any(a < d and c < b
                        for a, b in ranges for c, d in spans)]
        layout = bodies[address].get('instruction_spans') if body_ranges else None
        decoded_layout = [[i['address'], len(bytes.fromhex(i['bytes']))] for i in instructions]
        layout_matches = decoded_layout == layout if layout is not None else None
        record = dict(address=address, size=size, name=row['name'],
                      ghidra_instruction_layout_matches=layout_matches,
                      ghidra_instruction_bytes=(sum(length for _, length in layout)
                                                if layout is not None else None),
                      boundary=boundary, ranges=ranges,
                      overlapping_entries=sorted(overlaps),
                      undecodable_addresses=[i['address'] for i in instructions
                                             if i['mnemonic'] in ('(bad)', '.byte')],
                      instructions=instructions,
                      control_flow=control_flow_report(instructions))
        (output / (stem + '.json')).write_text(json.dumps(record, indent=2) + '\n')
        listing = [f'; {address:#010x} {row["name"]}: {boundary} ({size} bytes)']
        listing += [f'{i["address"]:08x}: {i["bytes"]:<30} {i["mnemonic"]} {i["operands"]}'.rstrip()
                    for i in instructions]
        (output / (stem + '.asm')).write_text('\n'.join(listing) + '\n')
        entries.append(dict(address=address, size=size, name=row['name'],
                            state=row['state'], kind=row['kind'], module=row['module'],
                            overlapping_entries=sorted(overlaps),
                            instruction_count=len(instructions),
                            review_edge_count=len(record['control_flow']['review_edges']),
                            ghidra_instruction_layout_matches=layout_matches,
                            file=stem + '.json'))
    manifest = dict(schema=3, payload_sha256=hashlib.sha256(payload).hexdigest(),
                    decoder=version, boundary=boundary, body_ranges_sha256=body_digest,
                    overlap_scope='selected-tracker-entries',
                    selected_rows_sha256=hashlib.sha256(json.dumps(
                        [row for _, _, row in sorted(prepared)], sort_keys=True).encode()).hexdigest(),
                    warning='Ghidra bodies and decoded windows are analysis hypotheses, not verified code.',
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
    parser.add_argument('--body-ranges', type=Path, help='payload-bound Ghidra body-ranges.json')
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
                                    output, objdump=args.objdump, body_ranges=args.body_ranges)
        print(f'Exported {len(manifest["entries"])} entries ({manifest["boundary"]}) to {output}')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_inventory: {error}\n')


if __name__ == '__main__':
    main()
