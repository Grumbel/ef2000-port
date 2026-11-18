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
  status   print a concise project progress report
  summary  print counts per state, kind and module
  next     propose the next functions to recover
  check    validate the tracker (used by nix flake check)
"""

import argparse
import collections
import csv
import hashlib
import json
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
SIDE_STATES = ('blocked', 'compiles-bad', 'library', 'data', 'dead')
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


def translation_blockers(rows, report, out=sys.stdout):
    """Cross-reference recorded generation results; never promote tracker evidence."""
    if report.get('model') != 'flat-x86-program-explicit-state':
        raise TrackerError('translation report has an unsupported model')
    entries = {}
    for entry in report['entries']:
        address = entry['address']
        if entry.get('state') not in ('generated-unverified', 'unsupported'):
            raise TrackerError('expected a C generation report from asm_c_program.py')
        if address in entries:
            raise TrackerError(f'duplicate translation entry: {format_addr(address)}')
        entries[address] = entry
    counts = collections.Counter()
    sizes = collections.Counter()
    roots = collections.Counter()
    service_dependencies = collections.Counter()
    incomplete_closure = 0
    for row in rows:
        if row['state'] != 'blocked':
            continue
        entry = entries.get(parse_addr(row['addr']))
        category = ('outside inventory' if entry is None else
                    'generated, unverified' if entry['state'] == 'generated-unverified'
                    else 'excluded by generator')
        counts[category] += 1
        sizes[category] += int(row['size'])
        if entry is not None and entry['state'] == 'generated-unverified':
            if entry.get('incomplete_direct_closure'):
                incomplete_closure += 1
            interrupts = entry.get('reachable_host_service_sites')
            ports = entry.get('reachable_host_port_service_sites')
            if interrupts or ports:
                service_dependencies['present'] += 1
            elif interrupts is None or ports is None:
                service_dependencies['unknown'] += 1
            else:
                service_dependencies['absent'] += 1
        if entry is not None:
            roots.update(set(entry.get('root_blockers', [])))
    print('Blocked tracker rows versus supplied translation report:', file=out)
    for category in ('generated, unverified', 'excluded by generator', 'outside inventory'):
        print(f'  {category:<25} {counts[category]:>6,} functions '
              f'{sizes[category]:>10,} body bytes', file=out)
    if incomplete_closure:
        print(f'  Of the generated functions, {incomplete_closure:,} have untranslated direct dependencies.',
              file=out)
    print('Generated blocked functions: direct interrupt or scalar port dependencies:', file=out)
    for key, label in (('present', 'reachable sites'), ('absent', 'no recorded reachable sites'),
                       ('unknown', 'incomplete reachability data')):
        print(f'  {label:<29} {service_dependencies[key]:>6,} functions', file=out)
    print('Direct-edge reachability excludes dynamic targets; it does not prove service support.', file=out)
    print('Direct dependency blockers (affected blocked functions; groups overlap):', file=out)
    print('Reasons are generator diagnostics. A code/data boundary error can appear as an instruction rejection.',
          file=out)
    for address, count in sorted(roots.items(), key=lambda item: (-item[1], item[0]))[:10]:
        reason = entries.get(address, {}).get('reason', 'absent from report')
        print(f'  {format_addr(address)} {count:>6,}  {reason}', file=out)
    print('Supplied report is a snapshot; source freshness, services and equivalence '
          'are not validated here. Tracker states are unchanged.', file=out)


def differential_summary(rows, translation_path, differential_path):
    """Validate report pairing and summarize coverage of generated blocked bodies."""
    source = translation_path.read_bytes()
    translation = json.loads(source)
    differential = json.loads(differential_path.read_bytes())
    if (differential.get('translation_report_sha256') != hashlib.sha256(source).hexdigest()
            or differential.get('payload_sha256') != translation.get('payload_sha256')):
        raise TrackerError('differential report does not match the supplied generation report')
    if differential.get('prepared_checkpoint') is not None:
        raise TrackerError('checkpoint comparisons cover prefixes only and cannot summarize full probes')
    generated = {entry['address']: entry for entry in translation['entries']
                 if entry['state'] == 'generated-unverified'}
    results = {}
    stops = {}
    stop_prefixes = (
        ('original service unavailable:', 'host service'),
        ('original fault:', 'original guest fault'),
        ('original instruction budget exhausted', 'instruction budget'),
        ('original reached STI stop', 'STI boundary'),
    )
    for entry in differential['entries']:
        address, state = entry['address'], entry['state']
        if (address in results or address not in generated
                or state not in ('probes-pass', 'inconclusive', 'mismatch')):
            raise TrackerError(f'invalid differential entry at {format_addr(address)}')
        results[address] = state
        if state == 'inconclusive':
            reasons = (probe.get('reason', '') for probe in entry.get('probes', [])
                       if probe.get('state') == 'inconclusive')
            stops[address] = {next((label for prefix, label in stop_prefixes
                                    if reason.startswith(prefix)), 'other')
                              for reason in reasons} or {'other'}
    blocked = collections.Counter()
    blocked_sizes = collections.Counter()
    blocked_stops = collections.Counter()
    for row in rows:
        address = parse_addr(row['addr'])
        if row['state'] == 'blocked' and address in generated:
            state = results.get(address, 'unmeasured')
            blocked[state] += 1
            blocked_sizes[state] += int(row['size'])
            blocked_stops.update(stops.get(address, ()))
    counts = collections.Counter()
    sizes = collections.Counter()
    missing_sizes = set()
    for address, entry in generated.items():
        state = results.get(address, 'unmeasured')
        counts[state] += 1
        size = entry.get('body_bytes')
        if type(size) is int and size > 0:
            sizes[state] += size
        else:
            missing_sizes.add(state)
    return translation, dict(blocked=blocked, blocked_sizes=blocked_sizes, blocked_stops=blocked_stops,
                             counts=counts, sizes=sizes, missing_sizes=missing_sizes)


def read_hybrid_excludes(path):
    """Addresses listed in decomp/symbols/hybrid-exclude.tsv (comments skipped)."""
    if not path.exists():
        return set()
    lines = [line for line in path.read_text().splitlines()
             if line.strip() and not line.startswith('#')]
    return {parse_addr(row['addr']) for row in csv.DictReader(lines, delimiter='\t')}


def status(rows, out=sys.stdout, recompilation=None, root=ROOT, translation=None,
           differential=None, link_report=None, byte_match=False):
    """Report recorded evidence, without treating compilation as equivalence."""
    total = len(rows)
    total_bytes = sum(int(row['size']) for row in rows)
    print('EF2000 decompilation — recorded tracker evidence', file=out)
    print(f'Inventory: {total:,} functions; {total_bytes:,} original body bytes', file=out)
    print(f'{"Milestone":<34} {"Functions":>10} {"Body bytes":>12} {"Bytes %":>8}', file=out)
    groups = (
        ('C/ASM draft or later (cumulative)', ('drafted', 'compiles', 'compiles-bad', 'spliced', 'verified')),
        ('Compiles or later (cumulative)', ('compiles', 'spliced', 'verified')),
        ('Spliced or verified (cumulative)', ('spliced', 'verified')),
        ('Verified equivalence', ('verified',)),
        ('Blocked', ('blocked',)),
        ('Compiled, known incorrect', ('compiles-bad',)),
        ('Library / data / dead', ('library', 'data', 'dead')),
    )
    for label, states in groups:
        selected = [row for row in rows if row['state'] in states]
        size = sum(int(row['size']) for row in selected)
        share = 100.0 * size / total_bytes if total_bytes else 0.0
        print(f'{label:<34} {len(selected):>10,} {size:>12,} {share:>7.1f}%', file=out)
    print('States: ' + ', '.join(f'{state}={sum(row["state"] == state for row in rows):,}'
                                for state in STATES if any(row['state'] == state for row in rows)), file=out)
    print('Counts describe tracker states, including C and assembly; blocked rows may contain drafts.', file=out)
    print('Bytes are summed original function body sizes, not unique image coverage or generated C size.', file=out)
    print('Compilation does not prove a match. Verified means recorded behavioral evidence.', file=out)
    excludes = read_hybrid_excludes(root / 'decomp/symbols/hybrid-exclude.tsv')
    candidates = [row for row in rows if row['state'] in ('compiles', 'spliced', 'verified')
                  and parse_addr(row['addr']) not in excludes]
    size = sum(int(row['size']) for row in candidates)
    share = 100.0 * size / total_bytes if total_bytes else 0.0
    print(f'{"Hybrid candidates (not excluded)":<34} {len(candidates):>10,} {size:>12,} {share:>7.1f}%',
          file=out)
    print('Hybrid candidates are compiles-or-later rows not in hybrid-exclude.tsv; an upper bound,'
          ' the linker drops more.', file=out)
    if link_report is not None:
        print(f"Link report: {link_report['functions_redirected']:,} functions redirected, "
              f"{link_report['excluded_functions']:,} excluded", file=out)
    from readability_metrics import collect
    metrics, _ = collect(root)
    print('Readability (decomp/src, lower is better): '
          f"literal={metrics['literal']:,} computed={metrics['computed']:,} "
          f"unnamed={metrics['unnamed']:,}; named globals={metrics['named_globals']:,}, "
          f"struct headers={metrics['struct_headers']:,}", file=out)
    if byte_match or recompilation is not None:
        recompilation_section(rows, out, recompilation, root)
    else:
        print('Byte-identical recompilation is frozen (plan rev 5); --byte-match shows it.', file=out)
    print('No recompilation or game tests run here.', file=out)
    if translation is not None:
        translation_blockers(rows, translation, out)
    if differential is not None:
        differential_section(differential, out)


def recompilation_section(rows, out, recompilation, root):
    from recompilation_status import current, read_report, DEFAULT_REPORT
    records = read_report(DEFAULT_REPORT if recompilation is None else recompilation)['records']
    counts = collections.Counter()
    sizes = collections.Counter()
    for row in rows:
        record = records.get(row['addr'])
        state = ('not-measured' if record is None else
                 record['state'] if current(record, row, root) else 'stale')
        counts[state] += 1
        sizes[state] += int(row['size'])
    print('Byte-identical recompilation (strict, resolved at original addresses):', file=out)
    for state in ('exact', 'different', 'not-comparable', 'not-measured', 'stale'):
        print(f'  {state:<18} {counts[state]:>8,} functions {sizes[state]:>10,} body bytes', file=out)
    print('Stale evidence is excluded from matches; a difference is not a behavioral failure.', file=out)


def differential_section(differential, out):
    states = ('probes-pass', 'inconclusive', 'mismatch', 'unmeasured')
    print('Prepared differential inputs for all emitted C bodies:', file=out)
    for state in states:
        size = ('unknown' if state in differential['missing_sizes']
                else f"{differential['sizes'][state]:,}")
        print(f"  {state:<18} {differential['counts'][state]:>8,} functions "
              f'{size:>10} body bytes', file=out)
    print('Prepared differential inputs for generated blocked functions:', file=out)
    for state in states:
        print(f"  {state:<18} {differential['blocked'][state]:>8,} functions "
              f"{differential['blocked_sizes'][state]:>10,} body bytes", file=out)
    if differential['blocked_stops']:
        print('Inconclusive blocked functions by stop reason (groups may overlap):', file=out)
        for reason, count in sorted(differential['blocked_stops'].items(),
                                    key=lambda item: (-item[1], item[0])):
            print(f'  {reason:<22} {count:>8,} functions', file=out)


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
        if state == 'compiles-bad':
            if not row['module']:
                errors.append(f'{where}: state compiles-bad requires a module')
            elif not (root / 'decomp/src' / f'{row["module"]}.c').is_file() and \
                    not (root / 'decomp/src' / f'{row["module"]}.asm').is_file():
                errors.append(f'{where}: missing decomp/src/{row["module"]}.c')
        if state in ('blocked', 'compiles-bad', 'dead', 'data') and not row['notes']:
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
    status_parser = sub.add_parser('status')
    status_parser.add_argument('--recompilation', type=Path,
                               help='override the committed byte-comparison report')
    status_parser.add_argument('--byte-match', action='store_true',
                               help='also show the frozen byte-identical recompilation records')
    status_parser.add_argument('--link-report', type=Path,
                               help='report JSON written by tools/hybrid_link.py --report')
    status_parser.add_argument('--translation-report', type=Path,
                               help='cross-reference a bulk translator report snapshot')
    status_parser.add_argument('--differential-report', type=Path,
                               help='summarize probes paired with --translation-report')
    next_parser = sub.add_parser('next')
    next_parser.add_argument('--module')
    next_parser.add_argument('--limit', type=int, default=10)
    next_parser.add_argument('--include-indirect', action='store_true',
                             help='also propose functions with indirect calls')
    next_parser.add_argument('--calls', type=Path,
                             default=DEFAULT_WORKSPACE / 'full-export/calls.tsv')
    sub.add_parser('check')
    args = parser.parse_args(argv)
    if args.command == 'status' and args.differential_report and not args.translation_report:
        parser.error('--differential-report requires --translation-report')
    try:
        if args.command == 'seed':
            added, updated, missing = seed(args.tracker, args.functions)
            print(f'tracker: {added} added, {updated} updated; {args.tracker}')
            for addr in missing:
                print(f'warning: {format_addr(addr)} is in the tracker but not in Ghidra',
                      file=sys.stderr)
            return 0
        rows = read_tracker(args.tracker)
        if args.command == 'status':
            differential = None
            if args.differential_report:
                translation, differential = differential_summary(
                    rows, args.translation_report, args.differential_report)
            else:
                translation = (json.loads(args.translation_report.read_text())
                               if args.translation_report else None)
            link_report = (json.loads(args.link_report.read_text())
                           if args.link_report else None)
            status(rows, recompilation=args.recompilation, translation=translation,
                   differential=differential, link_report=link_report,
                   byte_match=args.byte_match)
        elif args.command == 'summary':
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
    except (OSError, ValueError) as error:
        print(f'decomp_status: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
