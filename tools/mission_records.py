#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Inspect EF2000's loose, text-based mission/navigation/campaign records.

No executable code or original assets are bundled. This deliberately preserves
uninterpreted directives: field semantics and write-back are not recovered.
"""

import argparse
from collections import Counter
from dataclasses import dataclass
import json
from pathlib import Path
import shlex


MAX_INPUT_BYTES = 32 * 1024 * 1024
DECLARATIONS = {
    'NUM_MISSIONS': 'MISSION',
    'NUM_BASES': 'BASE',
    'NUM_BASE_NAVIGATIONS': 'BASE_NAVIGATION',
    'NUM_TARGET_AREAS': 'SSNAME',
    'NUM_MOBILES': 'MOB',
}
ROUTES = {
    'WP_ROUTE': 'WAYPOINT',
    'NAVIG_ROUTE': 'WAYPOINT',
    'ATTACK_ROUTE': 'ATTACK_NODE',
    'TIMESLOTS': 'TIMESLOT',
}


@dataclass(frozen=True)
class Record:
    line: int
    keyword: str
    args: tuple[str, ...]
    raw: str

    def json(self):
        return {'line': self.line, 'keyword': self.keyword,
                'args': list(self.args), 'raw': self.raw}


def parse_text(contents):
    """Tokenize records with quoted names and keep original text and line IDs.

    The original files use semicolon comment lines, sometimes mixed CR/LF.
    Quoted aircraft/callsign/airbase names may contain whitespace.
    """
    if len(contents) > MAX_INPUT_BYTES:
        raise ValueError('mission input too large')
    result = []
    for line_no, raw in enumerate(contents.splitlines(), 1):
        if not raw.strip() or raw.lstrip().startswith(';'):
            continue
        lexer = shlex.shlex(raw, posix=True)
        lexer.whitespace_split = True
        lexer.commenters = ';'
        try:
            tokens = list(lexer)
        except ValueError as error:
            raise ValueError(f'line {line_no}: {error}') from error
        if tokens:
            result.append(Record(line_no, tokens[0], tuple(tokens[1:]), raw))
    return result


def integer(record, arg_index, context):
    if len(record.args) <= arg_index:
        raise ValueError(f'line {record.line}: missing {context}')
    try:
        result = int(record.args[arg_index], 10)
    except ValueError as error:
        raise ValueError(f'line {record.line}: noninteger {context}') from error
    if result < 0 or result > 1000000:
        raise ValueError(f'line {record.line}: implausible {context}: {result}')
    return result


def inspect_records(records):
    """Report count/route invariants without inventing field interpretations."""
    counts = Counter(record.keyword for record in records)
    shapes = {}
    for record in records:
        shapes.setdefault(record.keyword, Counter())[len(record.args)] += 1
    declared = {}
    problems = []
    for record in records:
        if record.keyword in DECLARATIONS:
            if record.keyword in declared:
                problems.append(f'line {record.line}: duplicate {record.keyword}')
                continue
            number = integer(record, 0, record.keyword)
            declared[record.keyword] = number
            observed = counts[DECLARATIONS[record.keyword]]
            if number != observed:
                problems.append(f'line {record.line}: {record.keyword} declares '
                                f'{number}; observed {observed} {DECLARATIONS[record.keyword]}')

    route_totals = Counter()
    waypoint_totals = Counter()
    index_mismatches = Counter()
    waypoint_index_details = []
    for index, record in enumerate(records):
        if record.keyword not in ROUTES:
            continue
        children = ROUTES[record.keyword]
        expected = integer(record, 0, record.keyword)
        found = 0
        while (index + found + 1 < len(records) and
               records[index + found + 1].keyword == children):
            child = records[index + found + 1]
            if children == 'WAYPOINT':
                try:
                    point_index = int(child.args[0], 10)
                except (IndexError, ValueError):
                    point_index = None
                if point_index != found:
                    index_mismatches[record.keyword] += 1
                    waypoint_index_details.append({
                        'route_line': record.line, 'waypoint_line': child.line,
                        'expected_index': found, 'actual_index': point_index,
                    })
            found += 1
        route_totals[record.keyword] += 1
        waypoint_totals[record.keyword] += found
        if expected != found:
            problems.append(f'line {record.line}: {record.keyword} declares '
                            f'{expected} {children}; observed {found}')

    # Fourth argument of a BASE_NAVIGATION line is the number of corridors.
    # This is a count relationship observed in the supplied NAVIG.NAV, not a
    # claim about the remaining fields' meanings or units.
    bases = [pos for pos, record in enumerate(records)
             if record.keyword == 'BASE_NAVIGATION']
    for index, start in enumerate(bases):
        end = bases[index + 1] if index + 1 < len(bases) else len(records)
        count = sum(record.keyword == 'NAVIG_CORRIDOR' for record in records[start + 1:end])
        expected = integer(records[start], 3, 'BASE_NAVIGATION corridor count')
        if expected != count:
            problems.append(f'line {records[start].line}: BASE_NAVIGATION declares '
                            f'{expected} corridors; observed {count}')

    if 'NUM_MISSIONS' in declared:
        kind = 'mission'
    elif 'NUM_BASE_NAVIGATIONS' in declared:
        kind = 'navigation'
    elif 'NUM_TARGET_AREAS' in declared:
        kind = 'campaign'
    else:
        kind = 'unknown'

    # These are separately reported *observed shape relationships*, not
    # compatibility assumptions for every EF2000 release. Unlike explicit
    # NUM_/route lengths they do not make --strict reject a file.
    _, blocks = group_records(records, kind)
    block_checks = {}

    def track(name, block, good):
        result = block_checks.setdefault(name, {'checked': 0, 'mismatches': []})
        result['checked'] += 1
        if not good:
            result['mismatches'].append({'index': block['index'],
                                         'line': block['header'].line})

    for block in blocks:
        head = block['header']
        count = Counter(item.keyword for item in block['directives'])
        if block['kind'] == 'MISSION':
            track('mission_header_ordinal', block,
                  len(head.args) > 5 and head.args[5] == str(block['index']))
            track('mission_group_routes_and_targets', block,
                  count['GROUP'] + count['FLIGHT'] == count['WP_ROUTE'] == count['TARGET'])
            track('mission_flight_pylons', block, count['PYLON'] == 20 * count['FLIGHT'])
        elif block['kind'] == 'BASE':
            track('base_header_ordinal', block,
                  len(head.args) > 3 and head.args[3] == str(block['index']))
            track('base_timeslot_sections', block, count['TIMESLOTS'] == 1)
        elif block['kind'] == 'BASE_NAVIGATION':
            track('navigation_position_and_corridors', block,
                  count['3D_POSITION'] == 1 and
                  count['NAVIG_CORRIDOR'] == count['NAVIG_ROUTE'])
        elif block['kind'] == 'MOB':
            track('mobile_header_ordinal', block,
                  len(head.args) > 5 and head.args[5] == str(block['index']))
            track('mobile_pilot_record', block, count['PIL'] == 1)

    return {
        'kind': kind,
        'records': len(records),
        'declarations': declared,
        'counts': dict(sorted(counts.items())),
        'argument_counts': {key: dict(sorted(value.items()))
                            for key, value in sorted(shapes.items())},
        'routes': {name: {'count': route_totals[name],
                          'children': waypoint_totals[name],
                          'waypoint_index_mismatches': index_mismatches[name]}
                   for name in ROUTES if route_totals[name]},
        'waypoint_index_details': waypoint_index_details,
        'block_checks': block_checks,
        'problems': problems,
        'notes': ['Fields are syntactic tokens; unknown records are preserved.',
                  'No field units, game semantics, or editing/round-trip guarantees.'],
    }


def group_records(records, kind):
    """Reconstruct record boundaries while retaining every unparsed directive."""
    starters = {
        'mission': {'MISSION', 'BASE'},
        'navigation': {'BASE_NAVIGATION'},
        'campaign': {'SSNAME', 'MOB'},
    }.get(kind, set())
    counts = Counter()
    headers = []
    blocks = []
    current = None
    for record in records:
        if record.keyword in DECLARATIONS:
            headers.append(record)
            current = None
        elif record.keyword in starters:
            ordinal = counts[record.keyword]
            counts[record.keyword] += 1
            current = {'kind': record.keyword, 'index': ordinal,
                       'header': record, 'directives': []}
            blocks.append(current)
        elif current is not None:
            current['directives'].append(record)
        else:
            headers.append(record)
    return headers, blocks


def block_json(block):
    return {'kind': block['kind'], 'index': block['index'],
            'header': block['header'].json(),
            'directives': [record.json() for record in block['directives']]}


def inspect_bytes(data):
    if len(data) > MAX_INPUT_BYTES:
        raise ValueError('mission input too large')
    # Latin-1 preserves all input byte values; original text may be DOS OEM.
    # Actual glyph encoding and localized strings remain to be verified.
    records = parse_text(data.decode('latin-1'))
    report = inspect_records(records)
    return report, records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path, help='an original MISSION.CFG, NAVIG.NAV or CAMPAIGN.CG')
    parser.add_argument('--json', action='store_true', help='machine-readable inventory')
    parser.add_argument('--records-json', action='store_true',
                        help='emit source-line-referenced token records as JSON (can be large)')
    parser.add_argument('--blocks-json', action='store_true',
                        help='emit grouped mission/base/navigation/mobile records')
    parser.add_argument('--block', metavar='KIND:INDEX',
                        help='emit one grouped record, e.g. MISSION:516 or BASE_NAVIGATION:4')
    parser.add_argument('--strict', action='store_true',
                        help='fail if declared counts or route lengths disagree')
    args = parser.parse_args()
    try:
        if args.file.stat().st_size > MAX_INPUT_BYTES:
            raise ValueError('mission input too large')
        report, records = inspect_bytes(args.file.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    headers, blocks = group_records(records, report['kind'])
    if args.block:
        try:
            kind, number = args.block.split(':', 1)
            number = int(number)
            block = next((block for block in blocks
                          if block['kind'] == kind and block['index'] == number), None)
            if block is None:
                raise ValueError('block not found')
        except (ValueError, TypeError) as error:
            parser.exit(1, f'invalid --block {args.block!r}: {error}\n')
        print(json.dumps(block_json(block), indent=2))
    elif args.blocks_json:
        print(json.dumps({'inventory': report,
                          'unassigned_records': [record.json() for record in headers],
                          'blocks': [block_json(block) for block in blocks]}, indent=2))
    elif args.records_json:
        print(json.dumps({'inventory': report, 'records': [record.json() for record in records]},
                         indent=2))
    elif args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f'{args.file}: {report["kind"]}, {report["records"]:,} records')
        for name, number in report['declarations'].items():
            print(f'  {name} {number:,} -> {DECLARATIONS[name]} {report["counts"].get(DECLARATIONS[name], 0):,}')
        for name, route in report['routes'].items():
            print(f'  {name}: {route["count"]:,} routes, {route["children"]:,} child records, '
                  f'{route["waypoint_index_mismatches"]} index mismatches')
        print(f'  problems: {len(report["problems"])}')
        for problem in report['problems'][:20]:
            print(f'    {problem}')
        if len(report['problems']) > 20:
            print(f'    … {len(report["problems"]) - 20} more')
    if args.strict and report['problems']:
        parser.exit(1, f'{len(report["problems"])} count/route anomalies\n')


if __name__ == '__main__':
    main()
