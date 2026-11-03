#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Maintain the EF2000 decompilation function tracker.

The tracker, decomp/symbols/functions.tsv, records facts about original
functions (address, size, name) and the recovery state of each one. It never
contains original code bytes or Ghidra pseudo-C. See
docs/decompilation-plan.md, sections 3.2 and 4.

Commands:
  seed     merge a Ghidra full-export functions.tsv into the tracker
  summary  print counts per state, kind and module
  next     propose the next functions to recover
  check    validate the tracker (used by nix flake check)
"""

import argparse
import collections
import csv
import os
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TRACKER = ROOT / 'decomp/symbols/functions.tsv'
DEFAULT_WORKSPACE = Path(os.environ.get('EF2000_GHIDRA_WORKSPACE', str(ROOT / 'local/ghidra')))

COLUMNS = ('addr', 'size', 'name', 'module', 'state', 'kind', 'sig_status', 'test', 'notes')
# Ordered: a function moves left to right through the pipeline.
PIPELINE = ('unknown', 'named', 'typed', 'drafted', 'compiles', 'spliced', 'verified')
SIDE_STATES = ('blocked', 'library', 'data', 'dead')
STATES = PIPELINE + SIDE_STATES
KINDS = ('unclassified', 'game', 'asm', 'thunk', 'clib', 'x32', 'ail', 'vbe')
LIBRARY_KINDS = ('clib', 'x32', 'ail', 'vbe')
SIG_STATUSES = ('unverified', 'confirmed')
# States that need no further recovery before callers can rely on them.
DONE_STATES = ('verified', 'library', 'dead')
# Ghidra-generated placeholder names carry no evidence; keep the name empty.
AUTO_NAME = re.compile(r'^(FUN|thunk_FUN|LAB|SUB|candidate_orphan|candidate_direct_call|candidate_callback_target)_[0-9a-fA-F]+$')
MODULE_NAME = re.compile(r'^[a-z0-9_]+$')


class TrackerError(ValueError):
    pass


def parse_addr(text):
    try:
        value = int(text, 16)
    except ValueError as error:
        raise TrackerError(f'invalid address: {text!r}') from error
    if value < 0 or value > 0xffffffff:
        raise TrackerError(f'address out of range: {text!r}')
    return value


def format_addr(value):
    return f'0x{value:08x}'


def read_tracker(path):
    if not path.exists():
        return []
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if tuple(reader.fieldnames or ()) != COLUMNS:
            raise TrackerError(f'{path}: header must be: ' + '\t'.join(COLUMNS))
        return list(reader)


def write_tracker(path, rows):
    rows = sorted(rows, key=lambda row: parse_addr(row['addr']))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, COLUMNS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def new_row(addr, size, name):
    return {
        'addr': format_addr(addr), 'size': str(size), 'name': name, 'module': '',
        'state': 'named' if name else 'unknown', 'kind': 'unclassified',
        'sig_status': 'unverified', 'test': '', 'notes': '',
    }


def seed(tracker_path, functions_path):
    """Merge Ghidra's inventory; never discard manually recorded columns."""
    with functions_path.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not {'entry', 'name', 'body_bytes'} <= set(reader.fieldnames or ()):
            raise TrackerError(f'{functions_path}: not a Ghidra full-export functions.tsv')
        ghidra = {}
        for row in reader:
            addr = parse_addr(row['entry'])
            name = '' if AUTO_NAME.match(row['name']) else row['name']
            ghidra[addr] = (int(row['body_bytes']), name)
    existing = {parse_addr(row['addr']): row for row in read_tracker(tracker_path)}
    added = updated = 0
    for addr, (size, name) in ghidra.items():
        row = existing.get(addr)
        if row is None:
            existing[addr] = new_row(addr, size, name)
            added += 1
            continue
        if row['size'] != str(size):
            row['size'] = str(size)
            updated += 1
        if name and not row['name']:
            row['name'] = name
            if row['state'] == 'unknown':
                row['state'] = 'named'
            updated += 1
    missing = sorted(addr for addr in existing if addr not in ghidra)
    write_tracker(tracker_path, existing.values())
    return added, updated, missing


def summary(rows, out=sys.stdout):
    total_bytes = sum(int(row['size']) for row in rows)
    by_state = collections.Counter(row['state'] for row in rows)
    bytes_by_state = collections.Counter()
    for row in rows:
        bytes_by_state[row['state']] += int(row['size'])
    print(f'{len(rows)} functions, {total_bytes} body bytes', file=out)
    print('state        functions      bytes', file=out)
    for state in STATES:
        if by_state[state]:
            share = 100.0 * bytes_by_state[state] / total_bytes if total_bytes else 0.0
            print(f'{state:<12} {by_state[state]:>9} {bytes_by_state[state]:>10}'
                  f'  ({share:5.1f}%)', file=out)
    kinds = collections.Counter(row['kind'] for row in rows)
    print('kinds: ' + ', '.join(f'{kind}={kinds[kind]}' for kind in KINDS if kinds[kind]),
          file=out)
    modules = collections.Counter(row['module'] for row in rows if row['module'])
    if modules:
        print('modules: ' + ', '.join(f'{name}={count}'
                                      for name, count in sorted(modules.items())), file=out)
    done = sum(bytes_by_state[state] for state in DONE_STATES)
    if total_bytes:
        print(f'recovered or excluded: {100.0 * done / total_bytes:.1f}% of bytes', file=out)


def read_calls(path):
    callees = collections.defaultdict(set)
    indirect = collections.Counter()
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not {'caller_entry', 'callee_entry', 'kind'} <= set(reader.fieldnames or ()):
            raise TrackerError(f'{path}: not a Ghidra full-export calls.tsv')
        for row in reader:
            if not row['caller_entry']:
                continue
            caller = parse_addr(row['caller_entry'])
            if row['kind'] == 'direct' and row['callee_entry']:
                callee = parse_addr(row['callee_entry'])
                if callee != caller:
                    callees[caller].add(callee)
            elif row['kind'] != 'direct':
                indirect[caller] += 1
    return callees, indirect


def is_done(row):
    return row['state'] in DONE_STATES or row['kind'] in LIBRARY_KINDS


def propose(rows, callees, indirect, module=None, limit=10, include_indirect=False):
    """Return workable rows whose direct callees are all already done."""
    by_addr = {parse_addr(row['addr']): row for row in rows}
    workable = []
    for addr, row in by_addr.items():
        if row['state'] not in ('unknown', 'named', 'typed', 'drafted', 'compiles', 'spliced'):
            continue
        if row['kind'] in LIBRARY_KINDS:
            continue
        if module and row['module'] != module:
            continue
        if indirect[addr] and not include_indirect:
            continue
        pending = [callee for callee in callees.get(addr, ())
                   if callee not in by_addr or not is_done(by_addr[callee])]
        if pending:
            continue
        workable.append(row)
    # Finish started work first, then smallest functions.
    started = ('drafted', 'compiles', 'spliced')
    workable.sort(key=lambda row: (row['state'] not in started, int(row['size']),
                                   parse_addr(row['addr'])))
    return workable[:limit] if limit else workable


def check(rows, root=ROOT):
    errors = []
    previous = None
    for number, row in enumerate(rows, start=2):
        where = f'line {number} ({row.get("addr")})'
        try:
            addr = parse_addr(row['addr'])
        except TrackerError as error:
            errors.append(f'{where}: {error}')
            continue
        if row['addr'] != format_addr(addr):
            errors.append(f'{where}: address must be written as {format_addr(addr)}')
        if previous is not None and addr <= previous:
            errors.append(f'{where}: addresses must be unique and ascending')
        previous = addr
        if not row['size'].isdigit():
            errors.append(f'{where}: size must be a decimal byte count')
        if row['state'] not in STATES:
            errors.append(f'{where}: unknown state {row["state"]!r}')
        if row['kind'] not in KINDS:
            errors.append(f'{where}: unknown kind {row["kind"]!r}')
        if row['sig_status'] not in SIG_STATUSES:
            errors.append(f'{where}: unknown sig_status {row["sig_status"]!r}')
        if any('\n' in value or '\t' in value for value in row.values() if value):
            errors.append(f'{where}: fields must not contain tabs or newlines')
        if row['module'] and not MODULE_NAME.match(row['module']):
            errors.append(f'{where}: module must be lowercase [a-z0-9_]')
        state = row['state']
        if state in PIPELINE and PIPELINE.index(state) >= PIPELINE.index('drafted'):
            if not row['module']:
                errors.append(f'{where}: state {state} requires a module')
            elif not (root / 'decomp/src' / f'{row["module"]}.c').is_file() and \
                    not (root / 'decomp/src' / f'{row["module"]}.asm').is_file():
                errors.append(f'{where}: missing decomp/src/{row["module"]}.c')
        if state in ('typed',) + PIPELINE[PIPELINE.index('compiles'):] and \
                row['sig_status'] != 'confirmed':
            errors.append(f'{where}: state {state} requires sig_status=confirmed')
        if state == 'verified':
            if not row['test']:
                errors.append(f'{where}: verified functions need a test path')
            elif not (root / row['test']).is_file():
                errors.append(f'{where}: test file not found: {row["test"]}')
        if state in ('blocked', 'dead', 'data') and not row['notes']:
            errors.append(f'{where}: state {state} requires notes with the evidence')
        if state == 'library' and row['kind'] not in LIBRARY_KINDS:
            errors.append(f'{where}: state library requires a library kind')
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--tracker', type=Path, default=DEFAULT_TRACKER)
    sub = parser.add_subparsers(dest='command', required=True)
    seed_parser = sub.add_parser('seed')
    seed_parser.add_argument('--functions', type=Path,
                             default=DEFAULT_WORKSPACE / 'full-export/functions.tsv')
    sub.add_parser('summary')
    next_parser = sub.add_parser('next')
    next_parser.add_argument('--module')
    next_parser.add_argument('--limit', type=int, default=10)
    next_parser.add_argument('--include-indirect', action='store_true',
                             help='also propose functions with indirect calls')
    next_parser.add_argument('--calls', type=Path,
                             default=DEFAULT_WORKSPACE / 'full-export/calls.tsv')
    sub.add_parser('check')
    args = parser.parse_args(argv)
    try:
        if args.command == 'seed':
            added, updated, missing = seed(args.tracker, args.functions)
            print(f'tracker: {added} added, {updated} updated; {args.tracker}')
            for addr in missing:
                print(f'warning: {format_addr(addr)} is in the tracker but not in Ghidra',
                      file=sys.stderr)
            return 0
        rows = read_tracker(args.tracker)
        if args.command == 'summary':
            summary(rows)
        elif args.command == 'next':
            callees, indirect = read_calls(args.calls)
            for row in propose(rows, callees, indirect, args.module, args.limit,
                               args.include_indirect):
                print('\t'.join((row['addr'], row['size'], row['state'],
                                 row['name'] or '-', row['module'] or '-')))
        elif args.command == 'check':
            errors = check(rows)
            for error in errors:
                print(f'{args.tracker}: {error}', file=sys.stderr)
            if errors:
                return 1
            print(f'tracker ok: {len(rows)} functions')
    except (OSError, TrackerError) as error:
        print(f'decomp_status: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
