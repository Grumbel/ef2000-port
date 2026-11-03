#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Rank EF2000 function affinities from Ghidra facts without inventing subsystem labels.

Requires `ghidra_ef2000.py function-facts` and `export-all` in the same
workspace. Outputs are leads for manual review, NOT verified type inference.
"""

import argparse
from collections import Counter, defaultdict
import csv
from itertools import combinations
import json
import math
from pathlib import Path


FACT_COLUMNS = {'entry', 'name', 'body_bytes', 'instructions', 'direct_calls',
                'indirect_calls', 'conditional_branches', 'pcode_loads',
                'pcode_stores', 'unique_data_targets', 'data_reads', 'data_writes',
                'literal_address_candidates', 'cross_function_branches'}
REF_COLUMNS = {'entry', 'site', 'target', 'ref_type', 'source'}
CANDIDATE_COLUMNS = {'entry', 'site', 'target', 'operand', 'form', 'region'}
CALL_COLUMNS = {'caller_entry', 'callee_entry', 'kind'}
BRANCH_COLUMNS = {'entry', 'site', 'target', 'target_entry', 'kind'}
GUEST_MEMORY_END = 0x4de2f0


def rows(path, required):
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not required <= set(reader.fieldnames or ()):
            raise ValueError(f'invalid columns in {path.name}: missing {sorted(required - set(reader.fieldnames or ()))!r}')
        for line in reader:
            if None in line or any(line[field] is None for field in required):
                raise ValueError(f'malformed row in {path.name}')
            yield line


def guest_address(value):
    address = int(value, 0)
    if not 0 <= address < GUEST_MEMORY_END:
        raise ValueError(f'address outside supported guest window: {value}')
    return address


def load_facts(workspace):
    base = workspace / 'subsystem-facts'
    functions = {}
    for row in rows(base / 'function-facts.tsv', FACT_COLUMNS):
        entry = guest_address(row['entry'])
        if entry in functions:
            raise ValueError(f'duplicate function 0x{entry:x}')
        for column in FACT_COLUMNS - {'entry', 'name'}:
            if not row[column].isdigit():
                raise ValueError(f'invalid {column} for 0x{entry:x}')
        functions[entry] = row
    if not functions:
        raise ValueError('function facts are empty')
    report_path = workspace / 'full-export/report.json'
    if report_path.is_file():
        expected = json.loads(report_path.read_text())['discovered_functions']
        if len(functions) != expected:
            raise ValueError(f'function facts cover {len(functions)} of {expected} functions')

    references = defaultdict(set)
    candidates = defaultdict(set)
    for row in rows(base / 'data-references.tsv', REF_COLUMNS):
        entry, target = guest_address(row['entry']), guest_address(row['target'])
        if entry not in functions:
            raise ValueError(f'unknown referenced function 0x{entry:x}')
        references[entry].add(target)
    for row in rows(base / 'address-candidates.tsv', CANDIDATE_COLUMNS):
        entry, target = guest_address(row['entry']), guest_address(row['target'])
        if entry not in functions:
            raise ValueError(f'unknown candidate function 0x{entry:x}')
        if row['region'] not in ('initialized', 'runtime_candidate'):
            raise ValueError(f'unknown candidate region: {row["region"]}')
        candidates[entry].add(target)

    callees = defaultdict(set)
    direct_edges = set()
    for row in rows(workspace / 'full-export/calls.tsv', CALL_COLUMNS):
        if row['kind'] != 'direct' or not row['caller_entry'] or not row['callee_entry']:
            continue
        caller, callee = guest_address(row['caller_entry']), guest_address(row['callee_entry'])
        if caller in functions and callee in functions:
            callees[caller].add(callee)
            if caller != callee:
                direct_edges.add(tuple(sorted((caller, callee))))
    branch_edges = set()
    for row in rows(base / 'cross-function-branches.tsv', BRANCH_COLUMNS):
        if row['kind'] != 'cross_function_jump':
            raise ValueError(f'unknown cross-function branch kind: {row["kind"]}')
        caller = guest_address(row['entry'])
        target = guest_address(row['target_entry'])
        guest_address(row['site'])
        guest_address(row['target'])
        if caller not in functions or target not in functions or caller == target:
            raise ValueError(f'invalid cross-function branch: 0x{caller:x} -> 0x{target:x}')
        branch_edges.add(tuple(sorted((caller, target))))
    return functions, references, candidates, callees, direct_edges, branch_edges


def affinities(functions, references, candidates, callees, direct_edges, branch_edges):
    # Evidence classes are separate: a literal value that looks like an
    # address must never be upgraded to an established data reference.
    tokens = defaultdict(dict)
    for entry in functions:
        strong = references[entry]
        tentative = candidates[entry] - strong
        for address in strong:
            tokens[('exact', address)][entry] = 2.0
            key = ('region', address & ~0x3f)
            tokens[key][entry] = max(tokens[key].get(entry, 0), 0.5)
        for address in tentative:
            key = ('exact', address)
            tokens[key][entry] = max(tokens[key].get(entry, 0), 1.0)
            key = ('region', address & ~0x3f)
            tokens[key][entry] = max(tokens[key].get(entry, 0), 0.25)
        for callee in callees[entry]:
            tokens[('callee', callee)][entry] = 0.35

    scores = defaultdict(float)
    reasons = defaultdict(list)
    total = len(functions)
    for token, owners in tokens.items():
        if not 2 <= len(owners) <= 32:
            continue  # global utilities/common constants are stopwords
        idf = min(4.0, math.log2((total + 1) / (len(owners) + 1)))
        for left, right in combinations(sorted(owners), 2):
            pair = (left, right)
            points = min(owners[left], owners[right]) * idf
            scores[pair] += points
            if len(reasons[pair]) < 24:
                label = (f'{token[0]}:0x{token[1]:x}')
                reasons[pair].append(label)
    for pair in direct_edges:
        scores[pair] += 4.0
        reasons[pair].append('direct_call')
    for pair in branch_edges:
        scores[pair] += 4.0
        reasons[pair].append('cross_function_jump')
    return scores, reasons


def groups(functions, scores, threshold, maximum=16):
    """Greedy complete-linkage groups, not transitive connected components.

    Every pair in a merged group must independently meet the score threshold.
    This prevents one common utility from joining thousands of unrelated
    functions through a long chain of merely local affinities.
    """
    parent = {entry: entry for entry in functions}
    members = {entry: {entry} for entry in functions}

    def root(node):
        while parent[node] != node:
            parent[node] = parent[parent[node]]
            node = parent[node]
        return node

    for (left, right), score in sorted(scores.items(), key=lambda item: (-item[1], item[0])):
        if score < threshold:
            break
        a, b = root(left), root(right)
        if a == b or len(members[a]) + len(members[b]) > maximum:
            continue
        if any(scores.get(tuple(sorted((x, y))), 0) < threshold
               for x in members[a] for y in members[b]):
            continue
        keep, merge = min(a, b), max(a, b)
        parent[merge] = keep
        members[keep].update(members.pop(merge))
    return sorted((sorted(group) for group in members.values() if len(group) >= 2),
                  key=lambda m: (-len(m), m[0]))


def write_report(workspace, output, seeds, top=12, threshold=8.0):
    functions, references, candidates, callees, direct_edges, branch_edges = load_facts(workspace)
    scores, reasons = affinities(functions, references, candidates, callees,
                                direct_edges, branch_edges)
    output.mkdir(parents=True, exist_ok=True)
    neighbors = defaultdict(list)
    for (left, right), score in scores.items():
        neighbors[left].append((score, right, (left, right)))
        neighbors[right].append((score, left, (left, right)))
    for items in neighbors.values():
        items.sort(key=lambda item: (-item[0], item[1]))
    with (output / 'function-affinities.tsv').open('w', newline='') as stream:
        writer = csv.writer(stream, delimiter='\t')
        writer.writerow(('entry', 'name', 'neighbor', 'neighbor_name', 'score', 'reasons'))
        for entry in sorted(functions):
            for score, other, pair in neighbors[entry][:top]:
                writer.writerow((f'0x{entry:08x}', functions[entry]['name'],
                                 f'0x{other:08x}', functions[other]['name'], f'{score:.3f}',
                                 ' | '.join(reasons[pair])))

    components = groups(functions, scores, threshold)
    with (output / 'candidate-groups.tsv').open('w', newline='') as stream:
        writer = csv.writer(stream, delimiter='\t')
        writer.writerow(('group', 'size', 'label_hint', 'members'))
        for index, members in enumerate(components, 1):
            names = [functions[entry]['name'] for entry in members]
            named = [name for name in names if not name.startswith(('FUN_', 'candidate_'))]
            writer.writerow((index, len(members), named[0] if named else 'unclassified',
                             ','.join(f'0x{entry:08x}' for entry in members)))

    unknown = [seed for seed in seeds if seed not in functions]
    if unknown:
        raise ValueError('unrecognized seed entries: ' + ', '.join(f'0x{v:x}' for v in unknown))
    lines = [
        '# EF2000 static function-affinity leads', '',
        'Generated from Ghidra Listing references, instruction operands, direct calls',
        'and cross-function jumps.',
        '**These are candidate relationships, not confirmed subsystems or type assignments.**', '',
        f'- Recognized functions: {len(functions):,}',
        f'- Functions with explicit data references: {sum(bool(references[e]) for e in functions):,}',
        f'- Functions with literal address candidates: {sum(bool(candidates[e]) for e in functions):,}',
        f'- Distinct direct-call function pairs: {len(direct_edges):,}',
        f'- Distinct cross-function branch pairs: {len(branch_edges):,}',
        f'- Complete-linkage candidate groups with pairwise score ≥ {threshold:g}: '
        f'{len(components):,} (at most 16 members each)', '',
    ]
    for seed in seeds:
        lines += [f'## 0x{seed:08x}: {functions[seed]["name"]}', '',
                  '| Related function | Score | Evidence |', '| --- | ---: | --- |']
        for score, other, pair in neighbors[seed][:top]:
            why = ', '.join(reasons[pair][:6])
            lines.append(f'| `0x{other:08x}` {functions[other]["name"]} | {score:.1f} | {why} |')
        lines.append('')
    lines += ['## Interpretation', '',
              'Direct calls and jumps across function boundaries establish control flow,',
              'but not necessarily a shared gameplay subsystem. The complete-linkage',
              'grouping requires every pair of functions in a candidate group to meet',
              'the threshold and caps each group at 16 members; the partition is a lead,',
              'not a validated functional decomposition. Exact explicit data references are',
              'stronger evidence than matching literal address candidates. Shared 64-byte',
              'regions and common callees are weaker leads; common tokens are excluded',
              'as stopwords.', '']
    (output / 'seed-neighborhoods.md').write_text('\n'.join(lines))
    (output / 'summary.json').write_text(json.dumps({
        'functions': len(functions), 'data_reference_functions': sum(bool(v) for v in references.values()),
        'literal_candidate_functions': sum(bool(v) for v in candidates.values()),
        'direct_call_pairs': len(direct_edges),
        'cross_function_jump_pairs': len(branch_edges),
        'candidate_groups': len(components),
        'grouping': 'greedy complete-linkage, maximum 16 members',
        'strong_pair_threshold': threshold, 'seeds': [f'0x{s:08x}' for s in seeds],
    }, indent=2) + '\n')
    return len(functions), len(components)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True,
                        help='Ghidra workspace with full-export/ and subsystem-facts/')
    parser.add_argument('--output', type=Path,
                        help='default: workspace/subsystem-map/')
    parser.add_argument('--seed', type=guest_address, action='append', default=[],
                        help='repeat to display a function neighborhood')
    parser.add_argument('--top', type=int, default=12, help='maximum neighbors per function')
    parser.add_argument('--group-score', type=float, default=8.0,
                        help='minimum affinity for candidate group edges')
    args = parser.parse_args(argv)
    if args.top < 1 or not math.isfinite(args.group_score) or args.group_score <= 0:
        parser.error('--top and --group-score must be positive')
    try:
        count, communities = write_report(args.workspace.resolve(),
                                           (args.output or args.workspace / 'subsystem-map').resolve(),
                                           args.seed, args.top, args.group_score)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f'ghidra_subsystems: {error}\n')
    print(f'EF2000 subsystem affinity: {count} functions, {communities} candidate groups; '
          f'output: {(args.output or args.workspace / "subsystem-map").resolve()}')


if __name__ == '__main__':
    main()
