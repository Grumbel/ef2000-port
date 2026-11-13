#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compare generated explicit-state C with original x86 using deterministic probes.

Reports evidence per probe, never claims universal equivalence. Original faults
and fuel exhaustion are inconclusive, not passing tests. A C failure after the
original returns is a mismatch. All generated
artifacts and proprietary RAM remain under ignored local/.
"""

import argparse
from collections import Counter
import ctypes
import hashlib
import json
from pathlib import Path
import random
import subprocess

try:
    from . import asm_to_c
except ImportError:
    import asm_to_c


class State(ctypes.Structure):
    _fields_ = [('r', ctypes.c_uint32 * 8), ('flags', ctypes.c_uint32),
                ('pc', ctypes.c_uint32), ('fault', ctypes.c_uint32),
                ('memory', ctypes.POINTER(ctypes.c_uint8)), ('memory_size', ctypes.c_uint32)]


def probes(count, seed):
    rng = random.Random(seed)
    for index in range(count):
        values = [0, 1, 0x7f, 0x80, 0x7fff, 0x8000, 0x7fffffff, 0xffffffff]
        if index < 8:
            registers = [values[index]] * 8
        else:
            # Prepared pointer-bearing objects live in mapped scratch RAM.
            registers = [0x300000 + 0x100 * rng.randrange(16) for _ in range(8)]
        registers[4] = 0x3f0000
        flags = 0x202 | (rng.getrandbits(12) & 0x8d5)
        yield registers, flags


def compare(function, payload, address, registers, flags, *, budget=10000):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError
    from unicorn import x86_const

    memory_size, sentinel = 0x400000, 0x3ff000
    memory = bytearray(memory_size)
    memory[:len(payload)] = payload
    # Give all eight pointer probes deterministic object bytes, without
    # inventing semantic field types or substituting guest services.
    memory[0x300000:0x302000] = bytes(range(256)) * 32
    stack = registers[4]
    memory[stack:stack+4] = sentinel.to_bytes(4, 'little')
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, memory_size)
    cpu.mem_write(0, bytes(memory))
    for name, value in zip(asm_to_c.REGS, registers):
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
    cpu.reg_write(x86_const.UC_X86_REG_EFLAGS, flags)
    try:
        cpu.emu_start(address, sentinel, count=budget)
    except UcError as error:
        return dict(state='inconclusive', reason='original fault: ' + str(error))
    if cpu.reg_read(x86_const.UC_X86_REG_EIP) != sentinel:
        return dict(state='inconclusive', reason='original instruction budget exhausted')
    native_memory = (ctypes.c_uint8 * memory_size).from_buffer(memory)
    state = State((ctypes.c_uint32 * 8)(*registers), flags, address, 0,
                  native_memory, memory_size)
    status = function(ctypes.byref(state), budget)
    if status:
        return dict(state='mismatch', differences=[
            'C status %d (budget/pc/memory) after original completed' % status])
    differences = []
    expected = [cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + n.upper()))
                for n in asm_to_c.REGS]
    for name, actual, original in zip(asm_to_c.REGS, state.r, expected):
        if actual != original:
            differences.append('%s: C=0x%x x86=0x%x' % (name, actual, original))
    if state.pc != sentinel:
        differences.append('return PC')
    if state.flags != cpu.reg_read(x86_const.UC_X86_REG_EFLAGS):
        differences.append('flags: C=0x%x x86=0x%x' %
                           (state.flags, cpu.reg_read(x86_const.UC_X86_REG_EFLAGS)))
    original_ram = bytes(cpu.mem_read(0, memory_size))
    if memory != original_ram:
        first = next(i for i, (a, b) in enumerate(zip(memory, original_ram)) if a != b)
        differences.append('RAM first mismatch at 0x%x' % first)
    return dict(state='mismatch' if differences else 'pass', differences=differences)


def verify(directory, output, image, *, compiler='cc', scenarios=16, budget=10000,
           watcom=None):
    payload = image.read_bytes()
    manifest_path = directory / 'report.json'
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    if (manifest.get('model') != 'flat-x86-explicit-state' or
            manifest.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('translation payload provenance mismatch')
    if scenarios < 1 or budget < 1:
        raise ValueError('scenarios and budget must be positive')
    runtime = directory / 'ef_lift_state.h'
    if manifest.get('runtime_sha256') != hashlib.sha256(runtime.read_bytes()).hexdigest():
        raise ValueError('translation runtime digest mismatch')
    for entry in manifest['entries']:
        if entry['state'] == 'generated-unverified':
            if entry.get('source_sha256') != hashlib.sha256((directory / entry['file']).read_bytes()).hexdigest():
                raise ValueError('translation source digest mismatch')
    sources = [directory / e['file'] for e in manifest['entries']
               if e['state'] == 'generated-unverified']
    if not sources:
        raise ValueError('no generated C sources to test')
    output.mkdir(parents=True, exist_ok=False)
    watcom_results = {}
    if watcom:
        objects = output / 'objects'
        objects.mkdir()
        for source in sources:
            obj = objects / (source.stem + '.obj')
            result = subprocess.run([watcom, '-bt=dos', '-3r', '-za99', '-w1', '-we',
                                     '-i=' + str(directory), '-fo=' + str(obj), str(source)],
                                    capture_output=True, text=True)
            log = result.stdout + result.stderr
            (objects / (source.stem + '.log')).write_text(log)
            if result.returncode or not obj.is_file() or obj.stat().st_size == 0:
                raise ValueError('OpenWatcom compilation failed: ' + source.name)
            watcom_results[source.stem] = hashlib.sha256(obj.read_bytes()).hexdigest()
    library = output / 'translations.so'
    command = [compiler, '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-function', *map(str, sources), '-o', str(library)]
    result = subprocess.run(command, capture_output=True, text=True)
    (output / 'compile.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    loaded = ctypes.CDLL(str(library.resolve()))
    entries = []
    progress = output / 'progress.jsonl'
    for entry in manifest['entries']:
        if entry['state'] != 'generated-unverified':
            continue
        address = entry['address']
        function = getattr(loaded, 'ef_lift_%08x' % address)
        function.argtypes = [ctypes.POINTER(State), ctypes.c_uint32]
        function.restype = ctypes.c_uint
        results = [compare(function, payload, address, registers, flags, budget=budget)
                   for registers, flags in probes(scenarios, address)]
        counts = Counter(item['state'] for item in results)
        entries.append(dict(address=address, counts=dict(counts), probes=results,
                            watcom_object_sha256=watcom_results.get('ef_lift_%08x' % address),
                            state='mismatch' if counts['mismatch'] else
                            'probes-pass' if counts['pass'] == scenarios else 'inconclusive'))
        with progress.open('a') as stream:
            stream.write(json.dumps(entries[-1]) + '\n')
        if len(entries) % 25 == 0:
            print('Compared %d/%d entries' % (len(entries), len(sources)), flush=True)
    report = dict(schema=1, model=manifest['model'],
                  payload_sha256=manifest['payload_sha256'],
                  translation_report_sha256=hashlib.sha256(manifest_bytes).hexdigest(),
                  library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
                  scenarios=scenarios, instruction_budget=budget, entries=entries,
                  limits='Prepared states, flat segments, supported instructions only; no ABI or game integration proof.')
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--translations', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, default=asm_to_c.ROOT / 'local/ghidra')
    parser.add_argument('--compiler', default='cc')
    parser.add_argument('--watcom', help='also require warning-free OpenWatcom compilation')
    parser.add_argument('--scenarios', type=int, default=16)
    parser.add_argument('--budget', type=int, default=10000)
    args = parser.parse_args(argv)
    try:
        directory, output = args.translations.resolve(), args.output.resolve()
        if not all(p.is_relative_to(asm_to_c.ROOT / 'local') for p in (directory, output)):
            raise ValueError('translation and differential artifacts must stay under ignored local/')
        workspace = asm_to_c.asm_inventory.ghidra_ef2000.validated_workspace(args.workspace)
        report = verify(directory, output, workspace / 'payload.bin', compiler=args.compiler,
                        scenarios=args.scenarios, budget=args.budget, watcom=args.watcom)
        counts = Counter(e['state'] for e in report['entries'])
        print(dict(counts))
        return 1 if counts['mismatch'] else 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_c_differential: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
