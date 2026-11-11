#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Run the original EF2000 code with compiled recovered C hybrid patches."""

import argparse
import csv
import os
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    from . import extract_image, hybrid_link
except ImportError:
    import extract_image
    import hybrid_link


ROOT = Path(__file__).resolve().parent.parent


def build_patch(executable, object_dir, output):
    image = extract_image.extract(executable.read_bytes())[1]['payload.bin']
    objects = [(path, 'hand') for path in sorted(object_dir.glob('*.obj'))]
    if not objects:
        raise ValueError(f'no recovered OpenWatcom objects in {object_dir}')
    functions = hybrid_link.read_tsv(ROOT / 'decomp/symbols/functions.tsv')
    globals_ = hybrid_link.read_tsv(ROOT / 'decomp/symbols/globals.tsv')
    signatures = hybrid_link.read_tsv(ROOT / 'decomp/symbols/signatures.tsv')
    exclusions = hybrid_link.load_exclusions(
        ROOT / 'decomp/symbols/hybrid-exclude.tsv', len(image))
    blob, report = hybrid_link.link(
        objects, functions, globals_, {}, image,
        hybrid_link.load_abi_aliases(ROOT / 'decomp/include/ef_abi.h'),
        explicit_excludes=exclusions, signatures=signatures)
    output.write_bytes(blob)
    print('ef2000-hybrid: %d recovered C functions, %d redirect bytes, '
          '%d arena bytes; %d/%d modules linked' % (
              report['functions_redirected'], report['redirect_bytes'],
              report['arena_bytes'], report['linked_modules'],
              report['object_modules']), file=sys.stderr)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, nargs='?',
                        default=os.environ.get('EF2000_GAME_DIR'),
                        help='EF2000 installation directory or original _EF2000.EXE '
                             '(or set EF2000_GAME_DIR)')
    parser.add_argument('--objects', type=Path,
                        default=os.environ.get('EF2000_DECOMP_OBJECTS'),
                        help='directory of compiled OpenWatcom .obj files '
                             '(or set EF2000_DECOMP_OBJECTS)')
    parser.add_argument('--launcher', required=True, help=argparse.SUPPRESS)
    args, runner_args = parser.parse_known_args(argv)
    if args.objects is None:
        parser.error('provide --objects (build .#ef2000-decomp-watcom first) '
                     'or set EF2000_DECOMP_OBJECTS')
    if args.game is None:
        parser.error('provide the EF2000 installation directory or set EF2000_GAME_DIR')
    if runner_args[:1] == ['--']:
        runner_args = runner_args[1:]
    executable = args.game / '_EF2000.EXE' if args.game.is_dir() else args.game
    if not executable.is_file():
        parser.error(f'original _EF2000.EXE not found: {executable}')
    try:
        with tempfile.TemporaryDirectory(prefix='ef2000-decomp-') as directory:
            patch = Path(directory) / 'recovered.efpatch'
            build_patch(executable, args.objects, patch)
            return subprocess.call([args.launcher, str(executable), '--patch', str(patch),
                                    *runner_args])
    except (OSError, ValueError, csv.Error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    raise SystemExit(main())
