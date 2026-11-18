#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Count low-level guest-memory idioms left in the recovered C source.

The numbers are the readability measure of plan revision 5: they should
only go down. Each one is a place where recovered code still talks to the
original memory image instead of to named data or named functions.

  literal  EF_U8/U16/U32/S16/S32/F32/F64(<hex literal>): unnamed global
  computed EF_*(<expression>): pointer arithmetic on guest addresses,
           usually an unrecovered struct or array access
  unnamed  guest_fn_<hex> calls: callee not yet named or not yet linked
           as recovered C

Usage:
  python3 tools/readability_metrics.py                 # totals
  python3 tools/readability_metrics.py --by-module 20  # worst 20 modules
  python3 tools/readability_metrics.py --json          # machine-readable
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ACCESS = re.compile(r'\bEF_(?:U8|U16|U32|S16|S32|F32|F64)\(\s*')
LITERAL = re.compile(r'0x[0-9a-fA-F]+[uU]?[lL]?\s*\)')
UNNAMED = re.compile(r'\bguest_fn_[0-9a-fA-F]+')
STRUCT_HEADERS = 'decomp/include/ef_structs'
GLOBALS = 'decomp/symbols/globals.tsv'


def count_file(text):
    literal = computed = 0
    for match in ACCESS.finditer(text):
        if LITERAL.match(text, match.end()):
            literal += 1
        else:
            computed += 1
    return {
        'literal': literal,
        'computed': computed,
        'unnamed': len(UNNAMED.findall(text)),
    }


def collect(root):
    modules = {}
    for path in sorted((root / 'decomp/src').glob('*.c')):
        modules[path.stem] = count_file(path.read_text(errors='replace'))
    totals = {key: sum(m[key] for m in modules.values())
        for key in ('literal', 'computed', 'unnamed')}
    globals_path = root / GLOBALS
    named_globals = 0
    if globals_path.exists():
        lines = globals_path.read_text().splitlines()
        named_globals = sum(1 for line in lines[1:] if line.strip())
    struct_dir = root / STRUCT_HEADERS
    totals['named_globals'] = named_globals
    totals['struct_headers'] = len(list(struct_dir.glob('*.h'))) if struct_dir.exists() else 0
    totals['modules'] = len(modules)
    return totals, modules


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--by-module', type=int, metavar='N', default=0,
        help='list the N modules with the most remaining idioms')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)

    totals, modules = collect(args.root)
    if args.json:
        json.dump({'totals': totals, 'modules': modules}, sys.stdout, indent=1, sort_keys=True)
        print()
        return 0
    print('literal={literal} computed={computed} unnamed={unnamed} '
        'named_globals={named_globals} struct_headers={struct_headers} '
        'modules={modules}'.format(**totals))
    if args.by_module:
        ranked = sorted(modules.items(),
            key=lambda item: -(item[1]['literal'] + item[1]['computed'] + item[1]['unnamed']))
        for name, m in ranked[:args.by_module]:
            print('{:<32} literal={:<6} computed={:<6} unnamed={}'.format(
                name, m['literal'], m['computed'], m['unnamed']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
