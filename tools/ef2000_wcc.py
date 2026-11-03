#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compile one recovered C module to 32-bit OpenWatcom OMF.

The conservative bootstrap flags are provisional until the ABI audit in
decompilation-plan §3.5. No executable or proprietary material is linked.
"""

import argparse
import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'decomp/src'
# -s suppresses the default __CHK stack-probe call. Until we can resolve
# the original runtime symbols, generated game code must not add one.
# Initial functional-compatibility profile, not a claim about the game's
# original compiler switches. Avoid numerically unstable FP optimizations.
FLAGS = ('-bt=dos', '-3r', '-zp1', '-fpi87', '-fp3', '-ob', '-ot', '-s', '-wx', '-za99')


def compile_module(source, *, compiler='wcc386', output=None, execute=subprocess.run):
    source = source.resolve()
    if source.suffix != '.c' or not source.is_file():
        raise ValueError('expected an existing .c source file')
    # Fixtures and recovered source are the only supported inputs. Do not
    # permit arbitrary host code to masquerade as derived game source.
    # The wrapper itself lives in the read-only Nix store, but the user's
    # checked-out source and build outputs must stay in their working tree.
    work_root = Path(os.environ.get('EF2000_SOURCE', Path.cwd())).resolve()
    root = next((base for base in (work_root, ROOT) if
                 source.is_relative_to(base / 'decomp/src') or
                 source.is_relative_to(base / 'decomp/tests')), None)
    if root is None:
        raise ValueError('source must be under decomp/src or decomp/tests')
    destination = (output or work_root / 'decomp/build' / (source.stem + '.obj')).resolve()
    if destination.suffix != '.obj':
        raise ValueError('output must have a .obj extension')
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = [compiler, *FLAGS, '-i=' + str(root / 'decomp/include'),
               '-fo=' + str(destination), str(source)]
    execute(command, check=True)
    if not destination.is_file() or destination.stat().st_size == 0:
        raise ValueError('compiler returned without creating a nonempty OMF object')
    return destination


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='one decomp/src/*.c source')
    parser.add_argument('--compiler', default='wcc386', help='OpenWatcom wcc386 executable')
    parser.add_argument('--output', type=Path, help='OMF output (default: decomp/build/<stem>.obj)')
    args = parser.parse_args(argv)
    try:
        print(compile_module(args.source, compiler=args.compiler, output=args.output))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'ef2000-wcc: {error}\n')


if __name__ == '__main__':
    main()
