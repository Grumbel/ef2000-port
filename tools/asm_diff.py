#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Side-by-side original x86 and OpenWatcom OMF disassembly (debugging aid).

Comparison is for ABI/layout diagnosis only: function bytes need not match.
Ghidra's body byte count can include noncontiguous ranges, so the original
listing is explicitly a linear window from the function entry, not an exact
Ghidra function-body export.
"""

import argparse
import io
from pathlib import Path
import re
import subprocess
import tempfile

try:
    from . import decomp_status, ghidra_ef2000
except ImportError:
    import decomp_status
    import ghidra_ef2000


ORIGINAL_LINE = re.compile(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+(.+)$')


def original_listing(image, address, length, *, objdump='objdump', runner=subprocess.run):
    if not 0 <= address < ghidra_ef2000.IMAGE_SIZE:
        raise ValueError('guest address outside initialized image')
    end = min(address + length, ghidra_ef2000.IMAGE_SIZE)
    result = runner([objdump, '-b', 'binary', '-m', 'i386', '-M', 'intel', '-D',
                     '--start-address=' + hex(address), '--stop-address=' + hex(end),
                     str(image)], check=True, capture_output=True, text=True)
    return [line.strip() for line in result.stdout.splitlines()
            if ORIGINAL_LINE.match(line)]


def omf_listing(obj, *, wdis='wdis', runner=subprocess.run):
    with tempfile.TemporaryDirectory(prefix='ef2000-asm-diff-') as temporary:
        listing = Path(temporary) / 'object.asm'
        runner([wdis, str(obj), '-r', '-l=' + str(listing)], check=True,
               capture_output=True, text=True)
        if not listing.is_file() or listing.stat().st_size == 0:
            raise ValueError('wdis did not produce an OMF listing')
        return listing.read_text(errors='replace').splitlines()


def compare(address, obj, *, workspace, tracker, objdump='objdump', wdis='wdis',
            out=None, runner=subprocess.run):
    workspace = ghidra_ef2000.validated_workspace(workspace)
    rows = decomp_status.read_tracker(tracker)
    matches = [row for row in rows if decomp_status.parse_addr(row['addr']) == address]
    if len(matches) != 1:
        raise ValueError(f'address 0x{address:08x} not found in tracker')
    length = int(matches[0]['size'])
    if not 0 < length <= ghidra_ef2000.IMAGE_SIZE - address:
        raise ValueError('invalid function body size')
    left = original_listing(workspace / ghidra_ef2000.IMAGE_NAME, address, length,
                            objdump=objdump, runner=runner)
    right = omf_listing(obj, wdis=wdis, runner=runner)
    stream = out or io.StringIO()
    print(f'Original guest 0x{address:08x} (linear {length}-byte window)'
          f' | OpenWatcom OMF: {obj}', file=stream)
    print('Note: inspect relocated operands and differing function extents manually.', file=stream)
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else ''
        b = right[index] if index < len(right) else ''
        print(f'{a[:74]:<74} | {b}', file=stream)
    return stream.getvalue() if isinstance(stream, io.StringIO) else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('address', type=lambda s: int(s, 0))
    parser.add_argument('object', type=Path, help='OpenWatcom .obj')
    parser.add_argument('--workspace', type=Path,
                        default=ghidra_ef2000.ROOT / 'local/ghidra')
    parser.add_argument('--tracker', type=Path,
                        default=ghidra_ef2000.ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--objdump', default='objdump')
    parser.add_argument('--wdis', default='wdis')
    args = parser.parse_args(argv)
    try:
        if not args.object.is_file():
            raise ValueError(f'OMF object not found: {args.object}')
        compare(args.address, args.object, workspace=args.workspace,
                tracker=args.tracker, objdump=args.objdump, wdis=args.wdis,
                out=__import__('sys').stdout)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_diff: {error}\n')


if __name__ == '__main__':
    main()
