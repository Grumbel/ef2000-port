#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Compare original and recovered binary GUI parsing on local game resources.

Writes proprietary resources/fixtures only under ignored local/. Requires the
8-function parser/read/arena/list/copy/fill patch and Python Unicorn.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from did_archive import DIDArchive
from efpatch import decode

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--patch', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path,
                        default=ROOT / 'local/menu-resource-differential')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if not output.is_relative_to((ROOT / 'local').resolve()):
        parser.error('--output-dir must be under ignored local/')
    writes = decode(args.patch.read_bytes(), args.payload.read_bytes(), address_limit=0x4000000)
    required = (0xaa5c4, 0x9cde8, 0xa7b98, 0xaa1e4, 0xaa228,
                0xaa270, 0xdc832, 0xb3fb0)
    for entry in required:
        if not any(offset == entry and len(data) == 5 and data[0] == 0xe9
                   for offset, data in writes):
            parser.error(f'patch is missing required redirect 0x{entry:x}')
    output.mkdir(parents=True, exist_ok=True)
    spec = json.loads((ROOT / 'decomp/tests/menu_stream_memory_chain.json').read_text())
    template = spec['cases'][0]
    cases, resources = [], []
    for filename in ('DID.DAT', 'DID1ST.DAT'):
        source = args.game_dir / filename
        archive = DIDArchive(source.read_bytes())
        entry = archive.lookup('gui.par')
        if entry is None:
            parser.error(f'{source}: gui.par not found')
        data = archive.member(entry)
        case = copy.deepcopy(template)
        case['name'] = filename + ':gui.par'
        for initialization in case['memory']:
            address = int(initialization['address'], 0)
            if address == 0x1f0000:
                initialization['hex'] = data.hex()
            elif address == 0x40d1e4:
                initialization['hex'] = (0x1f0000 + len(data)).to_bytes(4, 'little').hex()
        cases.append(case)
        (output / (filename + '-gui.par')).write_bytes(data)
        resources.append({'archive': filename, 'resource': 'gui.par',
                          'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    spec['cases'] = cases
    fixture = output / 'cases.json'
    fixture.write_text(json.dumps(spec, indent=2) + '\n')
    result = subprocess.run([
        sys.executable, str(ROOT / 'tools/func_differential.py'), '0xaa5c4',
        '--flat-segments', '--spec', str(fixture), '--payload', str(args.payload),
        '--patch', str(args.patch)], capture_output=True, text=True)
    (output / 'comparison.log').write_text(result.stdout + result.stderr)
    (output / 'report.json').write_text(json.dumps(
        {'resources': resources, 'returncode': result.returncode,
         'scope': 'prepared RAM-backed GUI parser; not startup or rendered menu'},
        indent=2) + '\n')
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
