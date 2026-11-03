#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Summarize disassembly and two direct callers for an ABI sample worksheet.

All fields are mechanical observations, not asserted signatures. Linear
disassembly can include function holes, shared tails or padding; inspect the
Ghidra body and actual caller paths before confirming any convention.
"""

import argparse
import collections
import csv
from pathlib import Path
import re
import subprocess


FIELDS = ('entry', 'shape', 'distinct_callers', 'caller_a', 'caller_b',
          'linear_instructions', 'ret_forms', 'push_regs', 'pop_regs',
          'x87_instructions', 'rep_instructions', 'segment_or_io', 'status')
ASM = re.compile(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+\s*(.+)$', re.I)
X87 = re.compile(r'^f(?:ld|st|ild|ist|add|sub|mul|div|com|ucom|chs|abs|sqrt|sin|cos|ptan|patan|nst|init|wait|xch|clex|free)', re.I)
REP = re.compile(r'^rep(?:e|ne|z|nz)?\s', re.I)
REG = re.compile(r'^(?:e?[abcd]x|e?[sd]i|e?[sb]p)$', re.I)


def parse_disassembly(text):
    instructions = []
    for line in text.splitlines():
        matched = ASM.match(line)
        if matched:
            instructions.append(matched.group(1).strip().split('#', 1)[0].strip())
    push = set()
    pop = set()
    returns = set()
    x87 = rep = 0
    segment_or_io = False
    for instruction in instructions:
        words = instruction.lower().split(None, 1)
        if not words:
            continue
        opcode, operand = words[0], words[1].strip() if len(words) > 1 else ''
        if opcode == 'push' and REG.fullmatch(operand):
            push.add(operand)
        if opcode == 'pop' and REG.fullmatch(operand):
            pop.add(operand)
        if opcode.startswith('ret') or opcode == 'iret':
            returns.add(opcode + (' ' + operand if operand else ''))
        x87 += bool(X87.match(opcode))
        rep += bool(REP.match(instruction))
        if ('fs:' in instruction.lower() or 'gs:' in instruction.lower() or
                opcode in ('in', 'out', 'ins', 'outs', 'cli', 'sti')):
            segment_or_io = True
    return dict(linear_instructions=str(len(instructions)),
                ret_forms=','.join(sorted(returns)),
                push_regs=','.join(sorted(push)), pop_regs=','.join(sorted(pop)),
                x87_instructions=str(x87), rep_instructions=str(rep),
                segment_or_io='yes' if segment_or_io else 'no')


def analyze(samples, calls, image, *, objdump='objdump', runner=subprocess.run):
    image_size = len(image) if isinstance(image, (bytes, bytearray)) else image.stat().st_size
    caller_map = collections.defaultdict(lambda: collections.defaultdict(list))
    for row in calls:
        if row['kind'] == 'direct' and row['callee_entry'] and row['caller_entry']:
            caller_map[int(row['callee_entry'], 0)][int(row['caller_entry'], 0)].append(
                int(row['call_site'], 0))
    report = []
    for row in samples:
        address = int(row['entry'], 0)
        length = min(int(row['body_bytes']), 1024, image_size - address)
        if length <= 0:
            raise ValueError(f'bad guest body at {row["entry"]}')
        process = runner([objdump, '-b', 'binary', '-m', 'i386', '-M', 'intel', '-D',
                          f'--start-address=0x{address:x}',
                          f'--stop-address=0x{address + length:x}', str(image)],
                         check=True, capture_output=True, text=True)
        observed = parse_disassembly(process.stdout)
        callers = sorted(caller_map[address].items(),
                         key=lambda item: (-len(item[1]), item[0]))
        if len(callers) < 2:
            raise ValueError(f'{row["entry"]} has fewer than two distinct callers')
        report.append(dict(entry=row['entry'], shape=row['shape'],
                           distinct_callers=str(len(callers)),
                           caller_a=f'0x{callers[0][0]:08x}@0x{callers[0][1][0]:08x}',
                           caller_b=f'0x{callers[1][0]:08x}@0x{callers[1][1][0]:08x}',
                           **observed, status='needs_manual_abi_review'))
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('samples', type=Path)
    parser.add_argument('calls', type=Path)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', type=Path, help='review TSV')
    parser.add_argument('--objdump', default='objdump')
    args = parser.parse_args(argv)
    with args.samples.open(newline='') as stream:
        samples = list(csv.DictReader(stream, delimiter='\t'))
    with args.calls.open(newline='') as stream:
        calls = list(csv.DictReader(stream, delimiter='\t'))
    report = analyze(samples, calls, args.image, objdump=args.objdump)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        stream = args.output.open('w', newline='')
    else:
        import sys
        stream = sys.stdout
    try:
        writer = csv.DictWriter(stream, FIELDS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(report)
    finally:
        if args.output:
            stream.close()


if __name__ == '__main__':
    main()
