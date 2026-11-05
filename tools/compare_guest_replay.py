#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compare deterministic original and patched EF2000 startup guest state.

Private-input opt-in diagnostic, NOT a gameplay golden replay. With the
Unicorn C++ runner's --dos-clock guest option, hash all 64 MiB of guest RAM
except the explicitly changed code bytes. Fail if any baseline is itself
nondeterministic or if a patched execution differs in status, memory stats,
or any hash. A passing run does not prove transient/intermediate equivalence.
"""

import argparse
import re
import subprocess
from pathlib import Path

try:
    from .efpatch import apply_patches, decode
except ImportError:
    from efpatch import apply_patches, decode


IMAGE_SIZE = 0x4000000
MAX_HASH_RANGES = 32
MAX_HASH_LENGTH = 0x1000000
STATUS = re.compile(r'status=\w+ interrupts=\d+ ticks=\d+ irq0=\d+ '
                    r'ip=0x[0-9a-fA-F]+ ax=0x[0-9a-fA-F]+')
MEMORY = re.compile(r'^x32_memory: .+$', re.MULTILINE)
HASH = re.compile(r'^guest_hash: addr=0x([0-9a-fA-F]+) length=(\d+) '
                  r'sha256=([0-9a-f]{64})$', re.MULTILINE)
HIT = re.compile(r'^patch_hit: addr=0x([0-9a-fA-F]+) count=(\d+)$', re.MULTILINE)


def complementary_hash_ranges(writes, image_size=IMAGE_SIZE):
    """Split unchanged guest RAM into runner-supported 16 MiB SHA spans."""
    ranges = []
    cursor = 0
    for address, data in sorted(writes, key=lambda item: item[0]) + [(image_size, b'')]:
        if address < cursor or address > image_size or len(data) > image_size - address:
            raise ValueError('overlapping/out-of-range changed guest code')
        while cursor < address:
            length = min(MAX_HASH_LENGTH, address - cursor)
            ranges.append((cursor, length))
            cursor += length
        cursor += len(data)
    if len(ranges) > MAX_HASH_RANGES:
        raise ValueError('too many unchanged spans for runner hash limit')
    if not ranges:
        raise ValueError('no unchanged guest bytes to compare')
    return ranges


def parse_replay(text, ranges, expected_entries, *, patched):
    statuses = STATUS.findall(text)
    memories = MEMORY.findall(text)
    if len(statuses) != 1 or len(memories) != 1:
        raise ValueError('missing or duplicated replay status/memory statistics')
    observed = {}
    for address, length, digest in HASH.findall(text):
        key = (int(address, 16), int(length))
        if key in observed:
            raise ValueError('duplicate guest hash range')
        observed[key] = digest
    if set(observed) != set(ranges):
        raise ValueError('missing or unexpected guest hash ranges')
    if patched:
        hits = {int(addr, 16): int(count) for addr, count in HIT.findall(text)}
        if set(hits) != set(expected_entries) or any(not hits[addr] for addr in hits):
            raise ValueError('one or more candidate function entries were never reached')
        total = re.search(r'patch_hits: total=(\d+)', text)
        if total is None or int(total.group(1)) != sum(hits.values()):
            raise ValueError('missing or inconsistent patch-hit totals')
    return statuses[0], memories[0], observed


def run_replay(runner, exe, patches, ranges, timeout):
    arguments = [str(runner), str(exe)]
    for patch in patches:
        arguments += ['--patch', str(patch)]
    arguments += ['--instruction-limit', '20000000',
                  '--timing', 'instruction', '--dos-clock', 'guest', '--sound', 'off']
    for address, length in ranges:
        arguments += ['--hash-guest', f'0x{address:x}:{length}']
    result = subprocess.run(arguments, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout, check=False)
    if result.returncode != 0:
        raise ValueError(f'replay exited {result.returncode}: '
                         + result.stdout.decode(errors='replace')[-1600:])
    return result.stdout.decode(errors='replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--patch', type=Path, action='append', required=True)
    parser.add_argument('--baseline-runs', type=int, default=3)
    parser.add_argument('--candidate-runs', type=int, default=2)
    parser.add_argument('--timeout', type=int, default=120)
    args = parser.parse_args()
    if not 1 <= args.baseline_runs <= 16 or not 1 <= args.candidate_runs <= 16:
        parser.error('run counts must be 1..16')
    try:
        image = args.payload.read_bytes()
        if len(image) != 1662896:
            raise ValueError('expected 1,662,896-byte initialized original payload')
        apply_patches(image, args.patch)  # Validates every original digest and overlap.
        writes = [write for patch in args.patch for write in decode(patch.read_bytes(), image)]
        ranges = complementary_hash_ranges(writes)
        excluded = sum(len(data) for _, data in writes)
        entries = {address for address, _ in writes}
        baseline = None
        for i in range(args.baseline_runs):
            text = run_replay(args.runner.resolve(), args.executable.resolve(), [],
                              ranges, args.timeout)
            current = parse_replay(text, ranges, entries, patched=False)
            if baseline is not None and current != baseline:
                raise ValueError(f'original replay #{i + 1} is nondeterministic')
            baseline = current
        for i in range(args.candidate_runs):
            text = run_replay(args.runner.resolve(), args.executable.resolve(),
                              args.patch, ranges, args.timeout)
            current = parse_replay(text, ranges, entries, patched=True)
            if current != baseline:
                raise ValueError(f'patched replay #{i + 1} differs from original')
        print(f'replay: {args.baseline_runs} original and {args.candidate_runs} patched '
              f'runs identical; {len(ranges)} SHA-256 ranges, '
              f'{IMAGE_SIZE - excluded}/{IMAGE_SIZE} guest bytes compared '
              f'(only {excluded} patched bytes excluded)')
        print(f'replay: {baseline[0]}')
        print('replay: diagnostic startup only; no CD/gameplay golden coverage')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
