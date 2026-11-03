#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Resolve original EF2000 resource-lookup trace lines against DID archives.

The guest lookup trace records requests, including misses. This tool checks
whether each named resource actually exists in the selected archive and prints
its index hash, raw size, compression marker and decoded payload signature.
"""

import argparse
import ast
from pathlib import Path
import re
import sys

try:
    from .did_archive import DIDArchive
except ImportError:
    from did_archive import DIDArchive


LOOKUP_LINE = re.compile(r'^archive-lookup: archive=(DID1ST\.DAT|DID\.DAT|unknown) path=(".*")$')


def parse_request(line):
    match = LOOKUP_LINE.fullmatch(line.rstrip('\r\n'))
    if match is None:
        return None
    try:
        path = ast.literal_eval(match.group(2))
    except (SyntaxError, ValueError):
        return None
    if not isinstance(path, str):
        return None
    return match.group(1), path


def normalize_path(path):
    # DOS commonly passes .\relative\paths; the archive directory table
    # stores them without that prefix.
    path = path.replace('/', '\\')
    while path.startswith('.\\'):
        path = path[2:]
    return path


def resolve_request(archive, path):
    try:
        entry = archive.lookup(normalize_path(path))
    except (ValueError, UnicodeError) as error:
        return f'unsupported-path ({error})'
    if entry is None:
        return 'missing'
    head = archive.member_head(entry, 8)
    marker = 'RA' if archive.raw_member(entry).startswith(b'RA') else 'raw'
    return f'found hash={entry.hash:08x} size={entry.size} encoding={marker} head={head.hex()}'


def report(game_dir, lines, extensions=()):
    archives = {name: DIDArchive.from_path(game_dir / name)
                for name in ('DID1ST.DAT', 'DID.DAT')}
    counts = {'found': 0, 'missing': 0, 'unsupported-path': 0, 'unknown-archive': 0}
    seen = set()
    for line in lines:
        request = parse_request(line)
        if request is None or request in seen:
            continue
        seen.add(request)
        source, path = request
        if extensions and Path(normalize_path(path)).suffix.lower() not in extensions:
            continue
        if source not in archives:
            status = 'unknown-archive'
        else:
            status = resolve_request(archives[source], path)
        counts[status.split(' ', 1)[0]] += 1
        print(f'{source} {path}: {status}')
    print('summary: ' + ' '.join(f'{key}={count}' for key, count in counts.items()))
    return counts


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True,
                        help='original directory containing DID1ST.DAT and DID.DAT')
    parser.add_argument('--ext', action='append', default=[],
                        help='only show one extension; repeat for multiple (e.g. --ext .bbm)')
    parser.add_argument('trace', type=Path, nargs='?',
                        help='trace stderr log (default: stdin)')
    args = parser.parse_args(argv)
    extensions = tuple(ext.lower() if ext.startswith('.') else '.' + ext.lower()
                       for ext in args.ext)
    if args.trace:
        with args.trace.open(encoding='utf-8', errors='replace') as lines:
            report(args.game_dir, lines, extensions)
    else:
        report(args.game_dir, sys.stdin, extensions)


if __name__ == '__main__':
    main()
