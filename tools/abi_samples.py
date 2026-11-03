#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pick a reproducible, diverse 40-function ABI investigation worksheet.

The input is Ghidra's instruction-level function-facts.tsv plus the local
image. The recorded x87/REP *candidates* are byte heuristics, not proofs.
No game bytes, pseudo-C or disassembly are written to the TSV.
"""

import argparse
import collections
import csv
from pathlib import Path


FIELDS = ('entry', 'region', 'shape', 'body_bytes', 'instructions', 'direct_calls',
          'indirect_calls', 'conditional_branches', 'direct_callers', 'status')


def sample(rows, image, count=40, caller_counts=None):
    caller_counts = caller_counts or {}
    items = []
    for row in rows:
        entry = int(row['entry'], 0)
        size = int(row['body_bytes'])
        if size < 5 or not 0 <= entry < len(image):
            continue
        items.append((entry, size, row))
    items.sort()
    if len(items) < count:
        raise ValueError(f'need at least {count} candidates')
    selected = []
    used = set()
    region_count = 5
    # Five broad code-address regions; each receives eight distinct shapes.
    kinds = ('leaf', 'few_calls', 'indirect', 'branch_heavy', 'memory_heavy',
             'x87_candidate', 'rep_candidate', 'large')
    for region in range(region_count):
        lower = min(entry for entry, _, _ in items)
        upper = max(entry for entry, _, _ in items) + 1
        start = lower + (upper - lower) * region // region_count
        end = lower + (upper - lower) * (region + 1) // region_count
        options = [item for item in items if start <= item[0] < end]
        if not options:
            raise ValueError(f'no candidates in region {region}')
        # An ABI inference needs at least two callers. Favor functions with
        # two known direct callers, without silently dropping shapes for
        # which only indirect or single call sites are currently identified.
        preferred = [item for item in options if caller_counts.get(item[0], 0) >= 2]
        if preferred:
            options = preferred
        center = (start + end) // 2
        def qualifies(item, shape):
            entry, size, row = item
            calls = int(row['direct_calls'])
            indirect = int(row['indirect_calls'])
            branches = int(row['conditional_branches'])
            memory = int(row['pcode_loads']) + int(row['pcode_stores'])
            # Byte heuristics only, not instruction decoding; verify manually.
            body = image[entry:entry + min(size, 160)]
            return {
                'leaf': calls == 0 and indirect == 0,
                'few_calls': 1 <= calls <= 3 and indirect == 0,
                'indirect': indirect > 0,
                'branch_heavy': branches >= 5,
                'memory_heavy': memory >= 20,
                'x87_candidate': any(0xd8 <= byte <= 0xdf for byte in body),
                'rep_candidate': any(sequence in body for sequence in
                                     (b'\xf3\xa4', b'\xf3\xa5', b'\xf3\xaa',
                                      b'\xf3\xab', b'\xf2\xae', b'\xf3\xae')),
                'large': size >= 300,
            }[shape]
        for shape in kinds:
            candidates = [item for item in options if item[0] not in used and
                          qualifies(item, shape)]
            if not candidates:
                # Preserve regional coverage but explicitly label fallback.
                candidates = [item for item in options if item[0] not in used]
                label = 'fallback_for_' + shape
            else:
                label = shape
            if not candidates:
                raise ValueError(f'not enough distinct candidates in region {region}')
            chosen = min(candidates, key=lambda item: (abs(item[0] - center), item[0]))
            entry, size, row = chosen
            used.add(entry)
            selected.append(dict(entry=f'0x{entry:08x}', region=str(region),
                shape=label, body_bytes=str(size), instructions=row['instructions'],
                direct_calls=row['direct_calls'], indirect_calls=row['indirect_calls'],
                conditional_branches=row['conditional_branches'],
                direct_callers=str(caller_counts.get(entry, 0)),
                status='unreviewed'))
    if len(selected) != count or len(used) != count:
        raise ValueError('unexpected ABI sample size')
    return sorted(selected, key=lambda row: int(row['entry'], 0))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('facts', type=Path)
    parser.add_argument('image', type=Path)
    parser.add_argument('--calls', type=Path,
                        help='Ghidra full-export/calls.tsv for two-caller preference')
    parser.add_argument('--output', type=Path, help='optional review TSV destination')
    args = parser.parse_args(argv)
    with args.facts.open(newline='') as file:
        rows = list(csv.DictReader(file, delimiter='\t'))
    caller_sets = collections.defaultdict(set)
    if args.calls:
        with args.calls.open(newline='') as file:
            for row in csv.DictReader(file, delimiter='\t'):
                if row['kind'] == 'direct' and row['callee_entry'] and row['caller_entry']:
                    caller_sets[int(row['callee_entry'], 0)].add(int(row['caller_entry'], 0))
    counts = {entry: len(callers) for entry, callers in caller_sets.items()}
    report = sample(rows, args.image.read_bytes(), caller_counts=counts)
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
