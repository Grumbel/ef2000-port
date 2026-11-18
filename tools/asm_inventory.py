#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Export reproducible ASM inventories or audit observed boundary gaps under local/.

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
SCHEMA = 4
PREVIOUS_NORMALIZATION = 'x87-wait-opcode-split-v1'
NORMALIZATION = 'x87-wait-and-test-mov-overlap-v2'


def overlapping_test_moves(image, instructions, *, objdump='objdump', runner=subprocess.run):
    """Decode branch-targeted MOV r32,r32 embedded in TEST AX's immediate.

    Both decodings end at the same PC. No arbitrary interior entry or byte
    scanning is accepted, and the original TEST remains executable.
    """
    targets = {i.get('direct_target') for i in instructions
               if i['mnemonic'].startswith(('j', 'loop'))}
    additional = []
    for insn in instructions:
        encoded = bytes.fromhex(insn['bytes'])
        site = insn['address'] + 2
        if (insn['mnemonic'] != 'test' or len(encoded) != 4 or
                encoded[:3] != b'\x66\xa9\x89' or encoded[3] < 0xc0 or
                site not in targets):
            continue
        decoded = decode_window(image, site, 2, objdump=objdump, runner=runner)
        if (len(decoded) != 1 or decoded[0]['address'] != site or
                decoded[0]['bytes'] != encoded[2:].hex() or
                decoded[0]['mnemonic'] != 'mov'):
            raise ValueError('embedded MOV decode mismatch')
        additional.extend(decoded)
    return sorted(instructions + additional, key=lambda i: i['address'])


def wait_prefix_count(encoded):
    """Only normalize leading WAITs, optionally grouped with an x87 operation.

    9B is an independently executed instruction, not an x87 opcode modifier.
    Do not turn a bogus merged WAIT/non-x87 span into layout agreement.
    """
    count = 0
    while count < len(encoded) and encoded[count] == 0x9b:
        count += 1
    if not count or count == len(encoded):
        return count
    cursor = count
    while cursor < len(encoded) and encoded[cursor] in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67):
        cursor += 1
    return count if cursor < len(encoded) and 0xd8 <= encoded[cursor] <= 0xdf else 0


def normalized_layout(layout, payload):
    if layout is None:
        return None
    result = []
    for site, length in layout:
        count = wait_prefix_count(payload[site:site+length])
        result.extend([site+i, 1] for i in range(count))
        if count != length:
            result.append([site+count, length-count])
    return result


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
        count = wait_prefix_count(encoded)
        if count:
            if (offset < address or offset+len(encoded) > address+size or
                    image.read_bytes()[offset:offset+len(encoded)] != encoded):
                raise ValueError(f'disassembly byte/continuity mismatch at {offset:#x}')
            instructions.extend(dict(address=offset+i, bytes='9b', mnemonic='fwait', operands='')
                                for i in range(count))
            if count < len(encoded):
                # Re-decode the residual instruction; don't infer no-wait
                # mnemonics from objdump's fused spelling (e.g. FSTCW).
                instructions.extend(decode_window(image, offset+count, len(encoded)-count,
                                                  objdump=objdump, runner=runner))
        else:
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


def audit_trace_gaps(payload, body_ranges, trace_path):
    """Group observed entries into runs of PCs absent from the reviewed layout."""
    bodies, body_digest = load_body_ranges(body_ranges, payload)
    source = trace_path.read_bytes()
    document = json.loads(source)
    payload_digest = hashlib.sha256(payload).hexdigest()
    if (not isinstance(document, dict) or document.get('payload_sha256') != payload_digest or
            not isinstance(document.get('entries'), list)):
        raise ValueError('invalid or mismatched trace provenance')
    if any(body.get('instruction_spans') is None for body in bodies.values()):
        raise ValueError('trace audit requires instruction spans for every body')
    owned = {pc for body in bodies.values()
             for pc, _ in normalized_layout(body['instruction_spans'], payload)}
    gaps = {}
    for entry in document['entries']:
        if not isinstance(entry, dict) or type(entry.get('root')) is not int or not isinstance(entry.get('trace'), list):
            raise ValueError('invalid trace entry')
        root = entry['root']
        previous = None
        gap = None
        for step in entry['trace']:
            pc = step[0] if isinstance(step, list) and len(step) == 2 else step
            if type(pc) is not int or not 0 <= pc <= 0xffffffff:
                raise ValueError('invalid trace PC')
            if pc >= len(payload) or pc in owned:
                gap = None
            else:
                if gap is None:
                    key = (previous, pc)
                    if key not in gaps:
                        gaps[key] = dict(source=previous, target=pc, hits=0, roots=set(), observed_pcs=set())
                    gap = gaps[key]
                    gap['hits'] += 1
                    gap['roots'].add(root)
                gap['observed_pcs'].add(pc)
            previous = pc if pc < len(payload) else None
    entries = sorted(gaps.values(), key=lambda gap: (gap['target'], gap['source'] if gap['source'] is not None else -1))
    for gap in entries:
        gap['roots'] = sorted(gap['roots'])
        gap['observed_pcs'] = sorted(gap['observed_pcs'])
    return dict(schema=1, payload_sha256=payload_digest, body_ranges_sha256=body_digest,
                trace_sha256=hashlib.sha256(source).hexdigest(), entries=entries)


def export_inventory(image, rows, output, *, objdump='objdump', runner=subprocess.run,
                     body_ranges=None, skip_missing_bodies=False):
    """Caller supplies a validated payload. Never change tracker recovery states."""
    payload = image.read_bytes()
    bodies, body_digest = load_body_ranges(body_ranges, payload) if body_ranges else ({}, None)
    boundary = 'ghidra-body-ranges' if body_ranges else 'unverified-linear-window'
    omitted = []
    prepared = []
    seen = set()
    for row in rows:
        address, size = int(row['addr'], 16), int(row['size'])
        if address in seen:
            raise ValueError(f'duplicate entry {address:#x}')
        seen.add(address)
        if not body_ranges and (address < 0 or size <= 0 or address + size > len(payload)):
            raise ValueError(f'invalid linear window at {address:#x}, size {size}')
        if body_ranges and address not in bodies:
            if skip_missing_bodies:
                omitted.append(address)
                continue
            raise ValueError(f'no Ghidra body ranges for {address:#x}')
        ranges = bodies[address]['ranges'] if body_ranges else [[address, address + size]]
        prepared.append((address, ranges, row))
    # Refuse overwrite: stale files must not silently become part of a new run.
    output.mkdir(parents=True, exist_ok=False)
    if body_ranges:
        # Carry the hashed raw Ghidra evidence with the normalized inventory.
        (output / 'ghidra-body-ranges.json').write_bytes(body_ranges.read_bytes())
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
        normalized = normalized_layout(layout, payload)
        decoded_layout = [[i['address'], len(bytes.fromhex(i['bytes']))] for i in instructions]
        layout_matches = decoded_layout == normalized if layout is not None else None
        if layout_matches:
            instructions = overlapping_test_moves(image, instructions, objdump=objdump, runner=runner)
        record = dict(address=address, size=size, name=row['name'],
                      ghidra_instruction_layout_matches=layout_matches,
                      ghidra_instruction_spans=layout,
                      normalized_ghidra_instruction_spans=normalized,
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
    manifest = dict(schema=SCHEMA, payload_sha256=hashlib.sha256(payload).hexdigest(),
                    instruction_normalization=NORMALIZATION,
                    decoder=version, boundary=boundary, body_ranges_sha256=body_digest,
                    overlap_scope='selected-tracker-entries',
                    selected_rows_sha256=hashlib.sha256(json.dumps(
                        [row for _, _, row in sorted(prepared)], sort_keys=True).encode()).hexdigest(),
                    warning='Ghidra bodies and decoded windows are analysis hypotheses, not verified code.',
                    entries=entries)
    if skip_missing_bodies:
        manifest['missing_body_addresses'] = sorted(omitted)
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
    parser.add_argument('--skip-missing-bodies', action='store_true',
                        help='omit tracker entries without Ghidra bodies and list them in the manifest')
    parser.add_argument('--trace-gaps', type=Path, help='audit a payload-bound execution trace against --body-ranges')
    args = parser.parse_args(argv)
    if args.trace_gaps and (not args.body_ranges or args.address or args.skip_missing_bodies):
        parser.error('--trace-gaps requires --body-ranges and no inventory selection')
    if args.skip_missing_bodies and (not args.body_ranges or args.address):
        parser.error('--skip-missing-bodies requires --body-ranges and a bulk selection')
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ghidra_ef2000.ROOT / 'local'):
            raise ValueError('original ASM exports must stay under the ignored local/ directory')
        workspace = ghidra_ef2000.validated_workspace(args.workspace)
        if args.trace_gaps:
            payload = (workspace / ghidra_ef2000.IMAGE_NAME).read_bytes()
            report = audit_trace_gaps(payload, args.body_ranges, args.trace_gaps)
            with output.open('x') as stream:
                json.dump(report, stream, indent=2)
                stream.write('\n')
            print(f'Audited {len(report["entries"])} observed gap edges to {output}')
            return
        rows = decomp_status.read_tracker(args.tracker)
        if args.address:
            requested = set(args.address)
            rows = [row for row in rows if int(row['addr'], 16) in requested]
            missing = requested - {int(row['addr'], 16) for row in rows}
            if missing:
                raise ValueError(f'addresses absent from tracker: {sorted(missing)}')
        manifest = export_inventory(workspace / ghidra_ef2000.IMAGE_NAME, rows,
                                    output, objdump=args.objdump, body_ranges=args.body_ranges,
                                    skip_missing_bodies=args.skip_missing_bodies)
        print(f'Exported {len(manifest["entries"])} entries ({manifest["boundary"]}) to {output}')
        if manifest.get('missing_body_addresses'):
            print(f'Omitted {len(manifest["missing_body_addresses"])} entries without Ghidra bodies')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_inventory: {error}\n')


if __name__ == '__main__':
    main()
