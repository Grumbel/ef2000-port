#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Enumerate recovered EF2000 function entrypoints executed during DOS startup.

Opt-in: requires private original EXE and initialized X-32 payload. A chunked
EFPATCH1 file rewrites the *original byte to itself* at each candidate entry,
causing the Unicorn runner to count entries without changing guest code. This
is dynamic coverage, not a call graph, valid function-boundary proof, or a
substitute for tracing arbitrary computed branches.
"""

import argparse
import csv
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

try:
    from .efpatch import MAX_WRITES, pack
except ImportError:
    from efpatch import MAX_WRITES, pack


ROOT = Path(__file__).resolve().parents[1]
HIT = re.compile(r'^patch_hit: addr=0x([0-9a-fA-F]+) count=(\d+)$', re.MULTILINE)
STATUS = re.compile(r'\bstatus=(\w+) interrupts=(\d+) ticks=(\d+) irq0=(\d+) '
                    r'ip=(0x[0-9a-fA-F]+)')


def entries(functions, image_size):
    found = {}
    with functions.open(newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            address = int(row['addr'], 16)
            if address < image_size and int(row['size']) > 0:
                if address in found:
                    raise ValueError(f'duplicate entry 0x{address:x}')
                found[address] = {
                    'address': address,
                    'name': row['name'],
                    'state': row['state'],
                    'sig_status': row['sig_status'],
                    'module': row['module'],
                    'size': int(row['size']),
                }
    return found


def scan(image, indexed, exe, runner, budget, batch_size, timeout, workdir):
    functions = sorted(indexed)
    covered = {}
    normal_status = None
    for start in range(0, len(functions), batch_size):
        sample = functions[start:start + batch_size]
        patch_plan = {
            'format': 'ef2000-patch-v1',
            'base_image_sha256': hashlib.sha256(image).hexdigest(),
            'writes': [{'kind': 'inline', 'offset': address,
                        'bytes': image[address:address + 1].hex()} for address in sample],
        }
        path = workdir / 'startup-identities.efpatch'
        path.write_bytes(pack(patch_plan, image))
        command = [str(runner), str(exe), '--patch', str(path),
                   '--instruction-limit', str(budget), '--timing', 'instruction',
                   '--dos-clock', 'guest', '--sound', 'off']
        run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             timeout=timeout, check=False)
        output = run.stdout.decode('utf-8', errors='replace')
        if run.returncode != 0:
            raise ValueError(f'runner failed ({run.returncode}) at chunk {start}: '
                             + output[-2000:])
        statuses = STATUS.findall(output)
        if len(statuses) != 1:
            raise ValueError(f'missing or duplicated run status at chunk {start}')
        if normal_status is None:
            normal_status = statuses[0]
        elif statuses[0] != normal_status:
            raise ValueError(f'divergent identity replay status at chunk {start}')
        observed = {}
        for entry_hex, raw_count in HIT.findall(output):
            address = int(entry_hex, 16)
            if address not in sample or address in observed:
                raise ValueError(f'unexpected or duplicate hit 0x{address:x}')
            observed[address] = int(raw_count)
            if observed[address] > 0:
                covered[address] = observed[address]
        total = re.search(r'patch_hits: total=(\d+)', output)
        if total is None or sum(observed.values()) != int(total.group(1)):
            raise ValueError(f'inconsistent patch-hit totals at chunk {start}')
        print(f'chunk {start // batch_size + 1}: '
              f'{sum(bool(count) for count in observed.values())} entries, '
              f'{sum(observed.values())} calls', flush=True)
    return covered, normal_status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--functions', type=Path,
                        default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--instruction-limit', type=int, default=20_000_000)
    parser.add_argument('--batch-size', type=int, default=MAX_WRITES)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--report', type=Path,
                        help='optional JSON counts/metadata; no proprietary bytes')
    args = parser.parse_args()
    if not 1 <= args.batch_size <= MAX_WRITES:
        parser.error(f'--batch-size must be 1..{MAX_WRITES}')
    if args.instruction_limit <= 0:
        parser.error('--instruction-limit must be positive')
    try:
        image = args.payload.read_bytes()
        if len(image) != 1662896:
            raise ValueError('expected original 1,662,896-byte initialized payload')
        indexed = entries(args.functions, len(image))
        with tempfile.TemporaryDirectory(prefix='ef2000-startup-') as scratch:
            hit, status = scan(image, indexed, args.executable.resolve(),
                               args.runner.resolve(), args.instruction_limit,
                               args.batch_size, args.timeout, Path(scratch))
        found = [{**indexed[address], 'calls': count}
                 for address, count in sorted(hit.items())]
        print(f'\nstartup: {len(found)}/{len(indexed)} tracked entries executed; '
              f'{sum(hit.values())} total hits; status={status}')
        for row in sorted(found, key=lambda row: (-row['calls'], row['address']))[:40]:
            print(f"  0x{row['address']:06x} {row['calls']:8d}  "
                  f"{row['state']:<10s} {row['name'] or '<unknown>'}")
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps({
                'payload_sha256': hashlib.sha256(image).hexdigest(),
                'instruction_limit': args.instruction_limit,
                'timing': 'instruction', 'dos_clock': 'guest', 'sound': 'off',
                'status': status, 'tracked': len(indexed), 'executed': found,
            }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
