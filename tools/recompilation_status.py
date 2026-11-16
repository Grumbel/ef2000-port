#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Record strict, unmasked byte comparisons of reviewed contiguous functions.

Compiles the production module, resolves external OMF relocations at the
original entry address, and stores hashes and metadata only. Internal segment
relocations need layout proof and are reported as not comparable. The reviewed
code size excludes compiler alignment padding; never infer it from tracker size.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import subprocess

from ef2000_wcc import FLAGS, compile_module
from highscores_differential import load_image
from omf_splice import OMFError, read_module, parse_symbols, resolve_symbol

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPORT = ROOT / 'decomp/symbols/recompilation.json'
FORMAT = 'ef2000-recompilation-v1'
STATES = ('exact', 'different', 'not-comparable')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inputs(root, row):
    paths = [root / 'decomp/src' / (row['module'] + '.c')]
    paths += sorted((root / 'decomp/include').rglob('*'))
    paths += [root / p for p in ('decomp/symbols/signatures.tsv',
                                'decomp/symbols/globals.tsv', 'tools/ef2000_wcc.py',
                                'tools/omf_splice.py', 'tools/flirt_match.py',
                                'tools/recompilation_status.py')]
    return {str(p.relative_to(root)): digest(p.read_bytes()) for p in paths if p.is_file()}


def read_report(path):
    if not path.exists():
        return {'format': FORMAT, 'records': {}}
    report = json.loads(path.read_text())
    if report.get('format') != FORMAT or not isinstance(report.get('records'), dict):
        raise ValueError('invalid recompilation report')
    for addr, record in report['records'].items():
        if record.get('state') not in STATES or record.get('addr') != addr:
            raise ValueError(f'invalid recompilation record: {addr}')
    return report


def current(record, row, root):
    try:
        return (record['function'] == {k: row[k] for k in ('addr', 'name', 'size', 'module')}
                and record['inputs'] == inputs(root, row)
                and record['flags'] == list(FLAGS))
    except (OSError, KeyError):
        return False


def compare(module, row, code_size, image, functions, globals_):
    """Resolve external fixups only; never mask bytes or substitute a trampoline."""
    notes = row.get('notes', '').lower()
    if 'noncontiguous' in notes or 'discontiguous' in notes:
        raise OMFError('discontiguous original body needs explicit range mapping')
    pubs = [p for p in module.publics if p.symbol.rstrip('_') == row['name']]
    if len(pubs) != 1:
        raise OMFError('canonical public must resolve uniquely')
    public = pubs[0]
    segment = module.segments[public.segment]
    start, end = public.offset, public.offset + code_size
    if (segment.cls.upper() != 'CODE' or not segment.use32 or code_size <= 0
            or end > segment.size or not all(segment.written[start:end])):
        raise OMFError('invalid or uninitialized reviewed code extent')
    if any(p.segment == public.segment and start < p.offset < end for p in module.publics):
        raise OMFError('reviewed extent crosses another public')
    address = int(row['addr'], 16)
    old_size = int(row['size'])
    if address < 0 or address + old_size > len(image):
        raise OMFError('original body outside pinned image')
    data = bytearray(segment.data[start:end])
    touched = set()
    for fixup in module.fixups:
        if fixup.segment != public.segment:
            continue
        width = 4 if fixup.location == 9 else 2
        if fixup.offset >= end or fixup.offset + width <= start:
            continue
        if fixup.location != 9 or fixup.offset < start or fixup.offset + width > end:
            raise OMFError('unsupported or straddling relocation')
        if fixup.target_method != 2:
            raise OMFError('internal/group relocation needs original layout proof')
        flat = (fixup.frame_method == 1 and fixup.frame_index is not None
                and 0 < fixup.frame_index < len(module.groups)
                and module.groups[fixup.frame_index]['name'].upper() == 'FLAT')
        if fixup.frame_method not in (4, 5) and not flat:
            raise OMFError('frame requires segment-base proof')
        offset = fixup.offset - start
        occupied = set(range(offset, offset + 4))
        if touched & occupied:
            raise OMFError('overlapping relocations')
        touched |= occupied
        if not 0 < fixup.target_index < len(module.externs):
            raise OMFError('undefined external index')
        target = resolve_symbol(module.externs[fixup.target_index], functions, globals_)
        value = int.from_bytes(data[offset:offset + 4], 'little') + target + fixup.displacement
        if fixup.mode == 'self':
            value -= address + offset + 4
        elif fixup.mode != 'segment':
            raise OMFError('unsupported relocation mode')
        data[offset:offset + 4] = (value & 0xffffffff).to_bytes(4, 'little')
    original = image[address:address + old_size]
    return {'state': 'exact' if data == original else 'different',
            'compiled_bytes': len(data), 'original_sha256': digest(original),
            'linked_sha256': digest(data), 'reason': 'strict comparison; no masked bytes or padding added'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--address', required=True, type=lambda s: int(s, 0))
    parser.add_argument('--code-size', required=True, type=int,
                        help='reviewed compiled extent, excluding alignment padding')
    parser.add_argument('--reviewed-contiguous', action='store_true', required=True,
                        help='assert independent review of original contiguous body and compiled extent')
    parser.add_argument('--payload', required=True, type=Path)
    parser.add_argument('--compiler', default='wcc386')
    parser.add_argument('--report', type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()
    try:
        functions = parse_symbols(ROOT / 'decomp/symbols/functions.tsv')
        row = next(r for r in functions if int(r['addr'], 16) == args.address)
        image = load_image(payload_path=args.payload)
        compiler = Path(shutil.which(args.compiler) or args.compiler).resolve(strict=True)
        report = read_report(args.report)
        record = {'addr': row['addr'],
                  'function': {k: row[k] for k in ('addr', 'name', 'size', 'module')},
                  'inputs': inputs(ROOT, row), 'flags': list(FLAGS),
                  'compiler': str(compiler), 'compiler_sha256': digest(compiler.read_bytes()),
                  'payload_sha256': digest(image), 'reviewed_contiguous': True,
                  'reviewed_code_size': args.code_size}
        with tempfile.TemporaryDirectory(prefix='ef2000-recompile-') as temp:
            obj = compile_module(ROOT / 'decomp/src' / (row['module'] + '.c'),
                                 compiler=str(compiler), output=Path(temp) / 'module.obj')
            blob = obj.read_bytes()
            record['object_sha256'] = digest(blob)
            try:
                record.update(compare(read_module(blob), row, args.code_size, image, functions,
                                      parse_symbols(ROOT / 'decomp/symbols/globals.tsv')))
            except OMFError as error:
                record.update(state='not-comparable', reason=str(error))
        report['records'][row['addr']] = record
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
        print(f'{row["addr"]}: {record["state"]}; {record["reason"]}')
    except (OSError, ValueError, StopIteration, subprocess.CalledProcessError) as error:
        parser.exit(1, f'recompilation-status: {error}\n')


if __name__ == '__main__':
    main()
