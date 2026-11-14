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
import multiprocessing
from pathlib import Path
import random
import resource
import struct
import subprocess

try:
    from . import asm_to_c, asm_softfloat, asm_x87_oracle
except ImportError:
    import asm_to_c
    import asm_softfloat
    import asm_x87_oracle


class State(ctypes.Structure):
    _fields_ = [('r', ctypes.c_uint32 * 8), ('flags', ctypes.c_uint32),
                ('pc', ctypes.c_uint32), ('fault', ctypes.c_uint32),
                ('memory', ctypes.POINTER(ctypes.c_uint8)), ('memory_size', ctypes.c_uint32),
                ('segment', ctypes.c_uint32 * 6), ('x87', (ctypes.c_uint32 * 3) * 8),
                ('x87_control', ctypes.c_uint32), ('x87_status', ctypes.c_uint32),
                ('x87_tag', ctypes.c_uint32), ('x87_ip', ctypes.c_uint32),
                ('x87_cs', ctypes.c_uint32), ('x87_dp', ctypes.c_uint32),
                ('x87_ds', ctypes.c_uint32), ('x87_opcode', ctypes.c_uint32),
                ('x87_compare_profile', ctypes.c_uint32)]


X87_FIELDS = dict(x87_control='FPCW', x87_status='FPSW', x87_tag='FPTAG',
                  x87_ip='FIP', x87_cs='FCS', x87_dp='FDP', x87_ds='FDS', x87_opcode='FOP')


def capture_x87(cpu):
    """Capture physical raw-80 registers, including empty slots, and environment.

    This is the oracle's observable state, not a claim that Unicorn implements
    hardware exception status or instruction-environment updates correctly.
    """
    from unicorn import x86_const
    result = {field: cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + name))
              for field, name in X87_FIELDS.items()}
    result['x87'] = []
    for index in range(8):
        significand, exponent = cpu.reg_read(x86_const.UC_X86_REG_FP0 + index)
        result['x87'].append([significand & 0xffffffff, significand >> 32, exponent])
    return result


def bind_x87(cpu, initial=None):
    from unicorn import x86_const
    values = {field: 0 for field in X87_FIELDS}
    values.update(x87_control=0x37f, x87_tag=0xffff, x87=[[0, 0, 0] for _ in range(8)])
    if initial:
        values.update(initial)
    for index, (low, high, exponent) in enumerate(values['x87']):
        cpu.reg_write(x86_const.UC_X86_REG_FP0 + index, (low | (high << 32), exponent))
    for field, name in X87_FIELDS.items():
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name), values[field])
    return capture_x87(cpu)


def set_x87(state, snapshot):
    for field in X87_FIELDS:
        setattr(state, field, snapshot[field])
    for index, words in enumerate(snapshot['x87']):
        state.x87[index][:] = words


def bind_flat_segments(cpu):
    """Bind the zero-base selector profile shared by the original and C model."""
    from unicorn import x86_const
    table = 0x10000000
    cpu.mem_map(table, 0x1000)
    cpu.mem_write(table, bytes(8) + struct.pack('<QQQ', 0x00cf9a000000ffff,
                                              0x00cf92000000ffff, 0x00cf92000000ffff))
    cpu.reg_write(x86_const.UC_X86_REG_GDTR, (0, table, 31, 0))
    selectors = [16, 8, 16, 16, 16, 16]
    for name, value in zip(asm_to_c.SEGMENTS, selectors):
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
    return selectors


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


def compare(function, payload, address, registers, flags, *, budget=10000, program=False, phase=None,
            initial_x87=None, native_x87=None):
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
    selectors = bind_flat_segments(cpu)
    initial_fpu = bind_x87(cpu, initial_x87)
    binding = native_x87.bind(cpu) if native_x87 is not None else None
    def outcome(**fields):
        if binding is not None:
            fields['original_x87'] = binding.evidence()
        return fields
    for name, value in zip(asm_to_c.REGS, registers):
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
    cpu.reg_write(x86_const.UC_X86_REG_EFLAGS, flags)
    if phase:
        phase('original')
    try:
        cpu.emu_start(address, sentinel, count=budget)
    except asm_x87_oracle.Unavailable as error:
        return outcome(state='inconclusive', reason='original native x87 unavailable: '+str(error))
    except UcError as error:
        return outcome(state='inconclusive', reason='original fault: ' + str(error))
    if cpu.reg_read(x86_const.UC_X86_REG_EIP) != sentinel:
        return outcome(state='inconclusive', reason='original instruction budget exhausted')
    native_memory = (ctypes.c_uint8 * memory_size).from_buffer(memory)
    state = State((ctypes.c_uint32 * 8)(*registers), flags, address, 0,
                  native_memory, memory_size)
    state.segment[:] = selectors
    set_x87(state, initial_fpu)
    profile = (native_x87.comparison_profile if native_x87 is not None else
               asm_x87_oracle.unicorn_comparison_profile())['flags']
    state.x87_compare_profile = profile
    if phase:
        phase('c')
    status = (function(ctypes.byref(state), budget, sentinel) if program else
              function(ctypes.byref(state), budget))
    if status:
        reason = ('pending x87 exception delivery unsupported' if status == 8 else
                  'budget/pc/memory/divide')
        return outcome(state='mismatch', differences=[
            'C status %d (%s) at 0x%x after original completed' % (status, reason, state.pc)])
    if phase:
        phase('comparison')
    differences = []
    if state.x87_compare_profile != profile:
        differences.append('C comparison CPU profile changed')
    expected = [cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + n.upper()))
                for n in asm_to_c.REGS]
    for name, actual, original in zip(asm_to_c.REGS, state.r, expected):
        if actual != original:
            differences.append('%s: C=0x%x x86=0x%x' % (name, actual, original))
    for name, actual in zip(asm_to_c.SEGMENTS, state.segment):
        original = cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + name.upper()))
        if actual != original:
            differences.append('%s: C=0x%x x86=0x%x' % (name, actual, original))
    if state.pc != sentinel:
        differences.append('return PC')
    expected_fpu = capture_x87(cpu)
    for field in X87_FIELDS:
        if getattr(state, field) != expected_fpu[field]:
            differences.append('%s: C=0x%x x86=0x%x' %
                               (field, getattr(state, field), expected_fpu[field]))
    for index, words in enumerate(state.x87):
        if list(words) != expected_fpu['x87'][index]:
            differences.append('x87 physical FP%d raw-80 value' % index)
    if state.flags != cpu.reg_read(x86_const.UC_X86_REG_EFLAGS):
        differences.append('flags: C=0x%x x86=0x%x' %
                           (state.flags, cpu.reg_read(x86_const.UC_X86_REG_EFLAGS)))
    original_ram = bytes(cpu.mem_read(0, memory_size))
    if memory != original_ram:
        first = next(i for i, (a, b) in enumerate(zip(memory, original_ram)) if a != b)
        differences.append('RAM first mismatch at 0x%x' % first)
    return outcome(state='mismatch' if differences else 'pass', differences=differences)


def isolated_probes(function, payload, address, scenarios, budget, program, native_x87=None):
    """Keep oracle/native crashes local and distinguish the failing phase.

    No retry follows a crash: completed probes remain evidence and unexecuted
    probes are explicitly inconclusive. fork keeps the compiled library and
    payload immutable in the parent and avoids serializing ctypes pointers.
    """
    context = multiprocessing.get_context('fork')
    receiver, sender = context.Pipe(duplex=False)

    def work():
        try:
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            for index, (registers, flags) in enumerate(probes(scenarios, address)):
                sender.send(('phase', index, 'setup'))
                result = compare(function, payload, address, registers, flags, budget=budget,
                                 program=program, native_x87=native_x87,
                                 phase=lambda stage: sender.send(('phase', index, stage)))
                sender.send(('result', index, result))
        except Exception as error:
            sender.send(('error', -1, repr(error)))
        finally:
            sender.close()

    process = context.Process(target=work)
    process.start()
    sender.close()
    results, stage, index, error = [], 'setup', 0, None
    try:
        while True:
            kind, probe, value = receiver.recv()
            if kind == 'phase':
                index, stage = probe, value
            elif kind == 'result':
                if probe != len(results):
                    error = 'out-of-order probe result'
                results.append(value)
                stage = 'between-probes'
            else:
                error = value
    except EOFError:
        pass
    finally:
        receiver.close()
        process.join()
    if error:
        raise ValueError('comparison worker error: ' + error)
    if process.exitcode:
        if stage == 'original':
            results.append(dict(state='inconclusive', reason='original oracle worker exited %d at probe %d' %
                                (process.exitcode, index)))
        elif stage == 'c':
            results.append(dict(state='mismatch', differences=[
                'C worker exited %d after original completed at probe %d' % (process.exitcode, index)]))
        else:
            raise ValueError('comparison worker exited %d during %s' % (process.exitcode, stage))
        results.extend(dict(state='inconclusive', reason='not run after worker failure')
                       for _ in range(scenarios-len(results)))
    if len(results) != scenarios:
        raise ValueError('comparison worker returned incomplete evidence')
    return results


def verify(directory, output, image, *, compiler='cc', scenarios=16, budget=10000,
           watcom=None, softfloat_build=None, x87_oracle='unicorn'):
    if x87_oracle not in ('unicorn', 'hardware'):
        raise ValueError('unknown x87 oracle')
    payload = image.read_bytes()
    manifest_path = directory / 'report.json'
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    if manifest.get('state_abi') != asm_to_c.STATE_ABI:
        raise ValueError('incompatible translation state ABI; regenerate the C sources')
    program = manifest.get('model') == 'flat-x86-program-explicit-state'
    if (manifest.get('model') not in ('flat-x86-explicit-state', 'flat-x86-program-explicit-state') or
            manifest.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('translation payload provenance mismatch')
    if scenarios < 1 or budget < 1:
        raise ValueError('scenarios and budget must be positive')
    runtime = directory / 'ef_lift_state.h'
    if manifest.get('runtime_sha256') != hashlib.sha256(runtime.read_bytes()).hexdigest():
        raise ValueError('translation runtime digest mismatch')
    generated = [e for e in manifest['entries'] if e['state'] == 'generated-unverified']
    artifacts = generated + ([manifest['dispatcher']] if program else [])
    for entry in artifacts:
        if entry.get('source_sha256') != hashlib.sha256((directory / entry['file']).read_bytes()).hexdigest():
            raise ValueError('translation source digest mismatch')
    sources = [directory / e['file'] for e in artifacts]
    needs_narrow = any(asm_to_c.asm_x87.NARROW_DEFINE in source.read_text() for source in sources)
    needs_binary = any(asm_to_c.asm_x87.BINARY_DEFINE in source.read_text() for source in sources)
    expected_backends = [asm_to_c.asm_x87.NARROW_BACKEND] if needs_narrow else []
    if needs_binary:
        expected_backends.append(asm_to_c.asm_x87.BINARY_BACKEND)
    if manifest.get('required_backends', []) != expected_backends:
        raise ValueError('translation backend declaration mismatch')
    if needs_narrow:
        if softfloat_build is None:
            raise ValueError('narrowing translations require --softfloat-build')
        if manifest.get('backend_source_sha256') != asm_softfloat.digest(asm_to_c.ROOT / 'tools/asm_x87_narrow.c'):
            raise ValueError('narrowing adapter source digest mismatch; regenerate translations')
        asm_softfloat.validated_build(softfloat_build, watcom=bool(watcom))
    if needs_binary:
        if softfloat_build is None:
            raise ValueError('arithmetic translations require --softfloat-build')
        if manifest.get('binary_backend_source_sha256') != asm_softfloat.digest(asm_to_c.ROOT / 'tools/asm_x87_binary.c'):
            raise ValueError('arithmetic adapter source digest mismatch; regenerate translations')
        asm_softfloat.validated_build(softfloat_build, watcom=bool(watcom))
    if not generated:
        raise ValueError('no generated C sources to test')
    output.mkdir(parents=True, exist_ok=False)
    native_x87, native_report = (asm_x87_oracle.build(output/'original-x87', compiler=compiler)
                                 if x87_oracle == 'hardware' else (None, None))
    backend = (asm_softfloat.build_narrow(softfloat_build, output / 'narrow-backend',
                                        compiler=compiler, watcom=watcom) if needs_narrow else None)
    binary_backend = (asm_softfloat.build_binary(softfloat_build, output / 'binary-backend',
                                               compiler=compiler, watcom=watcom) if needs_binary else None)
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
    # The runtime has only stateless helpers. One translation unit shares those
    # helpers instead of reparsing/optimizing them in thousands of processes.
    # Watcom still compiles every original source as a separate object above.
    unit = output / 'translations.c'
    unit.write_text('\n'.join('#line 1 %s\n%s' % (json.dumps(str(source)), source.read_text())
                              for source in sources))
    command = [compiler, '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-function', '-I', str(directory), str(unit), '-o', str(library)]
    if backend:
        command += [str(output / 'narrow-backend/narrow.so'), '-Wl,-z,defs']
    if binary_backend:
        command += [str(output / 'binary-backend/binary.so'), '-Wl,-z,defs']
    result = subprocess.run(command, capture_output=True, text=True)
    (output / 'compile.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    loaded = ctypes.CDLL(str(library.resolve()))
    comparison_profile = (native_x87.comparison_profile if native_x87 is not None else
                          asm_x87_oracle.unicorn_comparison_profile())
    entries = []
    progress = output / 'progress.jsonl'
    for entry in manifest['entries']:
        if entry['state'] != 'generated-unverified':
            continue
        address = entry['address']
        function = getattr(loaded, 'ef_lift_run' if program else 'ef_lift_%08x' % address)
        function.argtypes = [ctypes.POINTER(State), ctypes.c_uint32] + ([ctypes.c_uint32] if program else [])
        function.restype = ctypes.c_uint
        results = isolated_probes(function, payload, address, scenarios, budget, program, native_x87=native_x87)
        counts = Counter(item['state'] for item in results)
        entries.append(dict(address=address, counts=dict(counts), probes=results,
                            watcom_object_sha256=watcom_results.get('ef_lift_%08x' % address),
                            state='mismatch' if counts['mismatch'] else
                            'probes-pass' if counts['pass'] == scenarios else 'inconclusive'))
        with progress.open('a') as stream:
            stream.write(json.dumps(entries[-1]) + '\n')
        if len(entries) % 25 == 0:
            print('Compared %d/%d entries' % (len(entries), len(generated)), flush=True)
    report = dict(schema=1, model=manifest['model'],
                  payload_sha256=manifest['payload_sha256'],
                  translation_report_sha256=hashlib.sha256(manifest_bytes).hexdigest(),
                  library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
                  host_translation_unit_sha256=hashlib.sha256(unit.read_bytes()).hexdigest(),
                  dispatcher_watcom_object_sha256=watcom_results.get('ef_lift_program'),
                  scenarios=scenarios, instruction_budget=budget, entries=entries,
                  process_isolation='fork-per-entry-with-phase-reporting',
                  state_abi=asm_to_c.STATE_ABI, segment_profile='zero-base-selectors-0..3-8-16-24',
                  x87_comparison='all physical raw-80 registers and environment; known oracle gaps remain',
                  original_x87_oracle=native_report or dict(model='unicorn'),
                  comparison_profile=comparison_profile,
                  narrow_backend=backend,
                  binary_backend=binary_backend,
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
    parser.add_argument('--softfloat-build', type=Path, help='validated SoftFloat build for narrowing stores and arithmetic')
    parser.add_argument('--x87-oracle', choices=('unicorn', 'hardware'), default='unicorn',
                        help='explicit independent native x87 coprocessor for original execution')
    parser.add_argument('--scenarios', type=int, default=16)
    parser.add_argument('--budget', type=int, default=10000)
    args = parser.parse_args(argv)
    try:
        directory, output = args.translations.resolve(), args.output.resolve()
        if not all(p.is_relative_to(asm_to_c.ROOT / 'local') for p in (directory, output)):
            raise ValueError('translation and differential artifacts must stay under ignored local/')
        workspace = asm_to_c.asm_inventory.ghidra_ef2000.validated_workspace(args.workspace)
        report = verify(directory, output, workspace / 'payload.bin', compiler=args.compiler,
                        scenarios=args.scenarios, budget=args.budget, watcom=args.watcom,
                        softfloat_build=args.softfloat_build.resolve() if args.softfloat_build else None,
                        x87_oracle=args.x87_oracle)
        counts = Counter(e['state'] for e in report['entries'])
        print(dict(counts))
        return 1 if counts['mismatch'] else 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'asm_c_differential: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
