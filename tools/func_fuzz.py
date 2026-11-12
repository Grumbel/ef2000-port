#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Randomised original-vs-patched differential runner over many functions.

``func_differential.py`` needs a hand-written spec per function.  This tool
generates seeded random register/stack-argument/memory scenarios instead:
register inputs are small integers, pointers into a pseudo-random data arena,
pointers into the real initialised game image, or arbitrary dwords.  Every
scenario runs the original and the patched image; scenarios in which the
original faults or exceeds the budget are skipped, and a function counts as
checked when at least ``--min-cases`` scenarios ran to completion.  Any
difference in the reviewed output/preserved registers, ESP cleanup or written
RAM (outside dead stack scratch) is a failure.

Like func_differential this is an ABI/RAM oracle, not a proof.
"""

import argparse
import json
from pathlib import Path
import random
import sys
from concurrent.futures import ProcessPoolExecutor

try:
    import func_differential as fd
except ImportError:  # imported as tools.func_fuzz
    from . import func_differential as fd

ARENA = 0x2000000
ARENA_SIZE = 0x20000
GUEST = 0x4000000
STACK = GUEST - 0x1000
SENTINEL = GUEST - 0x2000
SCRATCH = 0x10000

_STATE = {}


def _pick(rng, image_size):
    kind = rng.random()
    if kind < 0.30:
        return rng.choice((0, 1, 2, 3, 4, 5, 7, 8, 0x10, 0x20, 0xff, 0x100, 0x7fff, 0xffff,
                           0xffffffff, 0x80000000, rng.randrange(0, 512)))
    if kind < 0.65:
        return ARENA + rng.randrange(0, ARENA_SIZE - 0x400) & ~3
    if kind < 0.80:
        return rng.randrange(0x100000, min(image_size, 0x300000)) & ~3
    return rng.getrandbits(32)


def _arena(rng):
    data = bytearray(ARENA_SIZE)
    for i in range(0, ARENA_SIZE, 4):
        kind = rng.random()
        if kind < 0.5:
            value = rng.randrange(0, 256)
        elif kind < 0.7:
            value = ARENA + rng.randrange(0, ARENA_SIZE - 0x400) & ~3
        else:
            value = rng.getrandbits(32)
        data[i:i + 4] = value.to_bytes(4, 'little')
    return bytes(data)


def _init(executable, payload, patches):
    original = fd.load_image(executable=Path(executable) if executable else None,
                             payload_path=Path(payload) if payload else None)
    patched, _ = fd.apply_patches(original, [Path(p) for p in patches], GUEST)
    _STATE.update(original=original, patched=patched)


def _run(job):
    address, cases, seed, min_cases, budget, signatures, functions = job
    original, patched = _STATE['original'], _STATE['patched']
    try:
        signature = fd.read_signature(signatures, address)
        size = fd.read_function_extent(functions, address)
    except ValueError as error:
        return address, 'error', str(error), 0, 0
    rng = random.Random(seed ^ address)
    nargs = signature.stack_args_bytes // 4
    ran = skipped = 0
    fails = []
    soft = []
    for index in range(cases):
        arena = _arena(rng)
        regs = {name: _pick(rng, len(original)) for name in fd.INITIAL_REGS}
        args = [_pick(rng, len(original)) for _ in range(nargs)]
        scenario = (f'fuzz{index}', regs, args, [(ARENA, arena)])
        try:
            fd.execute(original, signature, scenario, GUEST, STACK, SENTINEL, budget, size)
        except (RuntimeError, ValueError, ImportError) as error:
            if 'x87' in str(error):
                return address, 'error', str(error), 0, 0
            skipped += 1
            continue
        try:
            diffs, _blocks, _written = fd.compare_case(
                original, patched, signature, scenario, GUEST, STACK, SENTINEL,
                SCRATCH, budget, size)
        except RuntimeError as error:
            diffs = [f'patched: {error}']
        ran += 1
        diffs = [d for d in diffs if not (d.startswith('ESP cleanup') and
                 len(set(d.rsplit('got ', 1)[1].split('/'))) == 1)]
        outs = {fd.register_part(part)[0] + str(fd.register_part(part)[1:])
                for part in signature.outputs}
        hard = [d for d in diffs if d.startswith(('guest RAM', 'ESP', 'patched:')) or
                (d.split(':')[0].lower() in {x.lower() for x in signature.outputs})]
        if hard:
            fails.append((index, hard))
            if len(fails) >= 3:
                break
        elif diffs:
            soft.append((index, diffs))
    if fails:
        return address, 'FAIL', '; '.join(f'{i}: {" | ".join(d)}' for i, d in fails), ran, skipped
    if soft and ran >= min_cases:
        return address, 'SOFT', '; '.join(f'{i}: {" | ".join(d)}' for i, d in soft[:2]), ran, skipped
    if ran < min_cases:
        return address, 'weak', f'only {ran} completed cases', ran, skipped
    return address, 'PASS', '', ran, skipped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    payload = parser.add_mutually_exclusive_group(required=True)
    payload.add_argument('--executable')
    payload.add_argument('--payload')
    parser.add_argument('--patch', action='append', required=True)
    parser.add_argument('--addresses', required=True,
                        help='file with one hex address per line')
    parser.add_argument('--cases', type=int, default=40)
    parser.add_argument('--min-cases', type=int, default=3)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--budget', type=int, default=20000)
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--output', required=True, help='TSV: addr status ran skipped detail')
    parser.add_argument('--signatures', default=str(fd.ROOT / 'decomp/symbols/signatures.tsv'))
    parser.add_argument('--functions', default=str(fd.ROOT / 'decomp/symbols/functions.tsv'))
    args = parser.parse_args()
    addresses = [int(line.split()[0], 0) for line in open(args.addresses) if line.strip()]
    jobs = [(a, args.cases, args.seed, args.min_cases, args.budget,
             args.signatures, args.functions) for a in addresses]
    counts = {}
    with ProcessPoolExecutor(args.jobs, initializer=_init,
                             initargs=(args.executable, args.payload, args.patch)) as pool, \
            open(args.output, 'w') as out:
        for address, status, detail, ran, skipped in pool.map(_run, jobs, chunksize=4):
            counts[status] = counts.get(status, 0) + 1
            out.write(f'0x{address:08x}\t{status}\t{ran}\t{skipped}\t{detail}\n')
            out.flush()
    print(json.dumps(counts))
    return 0


if __name__ == '__main__':
    sys.exit(main())
