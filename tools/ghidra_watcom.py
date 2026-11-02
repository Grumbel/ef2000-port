#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Prepare a private Ghidra copy with an experimental EF2000 Watcom model.

Only a *new destination* is modified. The installed Ghidra distribution and
every existing project are preserved, including read-only Nix store packages.
"""

import argparse
import os
from pathlib import Path
import shutil
import stat
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
CSPEC = Path('Ghidra/Processors/x86/data/languages/x86win.cspec')
TEMPLATE = ROOT / 'tools/ghidra/ef2000_watcom_prototype.xml'
MODEL = '__ef2000_aedb'
SENTINEL = '  <resolveprototype name="__fastcall/__thiscall/__stdcall">'


def inject_model(spec, model):
    """Insert precisely one model without rewriting standard Ghidra prototypes."""
    if spec.count(SENTINEL) != 1:
        raise ValueError('unsupported Ghidra x86 Windows compiler spec layout')
    original = ET.fromstring(spec)
    candidate = ET.fromstring(model)
    if candidate.tag != 'prototype' or candidate.attrib.get('name') != MODEL:
        raise ValueError('invalid experimental model template')
    if any(entry.attrib.get('name') == MODEL for entry in original.iter('prototype')):
        raise ValueError('the Ghidra compiler spec already contains the EF2000 model')
    result = spec.replace(SENTINEL, model + '\n' + SENTINEL)
    ET.fromstring(result)  # Abort before copying a distribution on XML errors.
    return result


def prepare(source, destination):
    source = source.expanduser().resolve()
    destination = destination.expanduser().resolve()
    source_cspec = source / CSPEC
    if not (source / 'support/analyzeHeadless').is_file() or not source_cspec.is_file():
        raise ValueError('expected a Ghidra installation root containing support/analyzeHeadless')
    if destination.exists() or destination.is_symlink():
        raise ValueError(f'destination already exists; refusing to overwrite: {destination}')
    if destination == source or destination.is_relative_to(source) or source.is_relative_to(destination):
        raise ValueError('Ghidra source and destination must be separate directories')
    modified = inject_model(source_cspec.read_text(), TEMPLATE.read_text())
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Follow source symlinks: never chmod/write through a link back into the
    # original installation or immutable Nix store after copying.
    shutil.copytree(source, destination, symlinks=False)
    target = destination / CSPEC
    target.chmod(target.stat().st_mode | stat.S_IWUSR)
    target.write_text(modified)
    print(f'Experimental Ghidra copy: {destination}')
    print(f'Headless launcher: {destination / "support/analyzeHeadless"}')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path,
                        default=Path(os.environ['GHIDRA_HOME']) if 'GHIDRA_HOME' in os.environ else None,
                        help='original Ghidra installation (default: GHIDRA_HOME)')
    parser.add_argument('--destination', type=Path,
                        default=ROOT / 'local/ghidra-watcom-install',
                        help='new, non-existing disposable Ghidra copy')
    args = parser.parse_args(argv)
    if args.source is None:
        parser.error('set --source or GHIDRA_HOME')
    try:
        prepare(args.source, args.destination)
    except (OSError, ValueError, ET.ParseError) as error:
        parser.exit(1, f'ghidra_watcom: {error}\n')


if __name__ == '__main__':
    main()
