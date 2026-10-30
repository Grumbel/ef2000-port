#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run original X-32 startup with native x86-64 or QEMU execution."""

import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import resource
import re
import shutil
import subprocess

try:
    from .extract_image import extract
except ImportError:
    from extract_image import extract


def analyze_calls(log):
    calls = []
    returns = []
    stops = []
    faults = []
    checkpoint = None
    allocation = None
    for line in log.splitlines():
        values = {key: int(value, 16) for key, value in re.findall(r'(\w+)=([0-9a-f]{8})', line)}
        if line.startswith('CALL '):
            calls.append(values)
        elif line.startswith('RETURN '):
            returns.append(values)
        elif line.startswith('STOP '):
            stops.append(values)
        elif line.startswith('FAULT '):
            faults.append(values)
        elif line.startswith('CHECK '):
            checkpoint = values
        elif line.startswith('ALLOCATION '):
            allocation = values
    sequence = [(item.get('IP'), item.get('AX')) for item in calls]
    verified = (sequence[:3] == [(0x93, 0x3505), (0xa0, 0x3005), (0x19a, 0x3509)] and
                len(returns) >= 3 and
                [(item.get('IP'), item.get('AX')) for item in returns[:3]] == [(0x93, 0x3505), (0xa0, 5), (0x19a, 0x3509)] and
                returns[0].get('CX') == 0x260 and returns[0].get('FLAGS', 0) & 0x41 == 0x40 and
                returns[1].get('BX') == 0 and returns[1].get('CX') == 0 and
                calls[2].get('CX') == 0x4000 and calls[2].get('SI') == 0x4df000 and
                calls[2].get('DI') == 0x4de2f0 and calls[2].get('SS') == 7 and
                calls[2].get('SP') == calls[2].get('BX') and
                allocation is not None and returns[2].get('DX') == allocation.get('STACK_LOW') and
                checkpoint == {'DATA_LIMIT': 0x260, 'BSS_ZERO': 1, 'COMMAND': 1,
                               'PROGRAM': 1, 'PSP': 0xf, 'ENV': 0x17})
    return {'calls': calls, 'returns': returns, 'stops': stops, 'faults': faults,
            'checkpoint': checkpoint, 'allocation': allocation, 'startup_checkpoint_verified': verified}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    version_file = Path(__file__).resolve().parent.parent / 'VERSION'
    version = os.environ.get('EF2000_VERSION') or version_file.read_text().strip()
    parser.add_argument('--version', action='version', version=f'ef2000 {version}')
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=['native64', 'qemu'], default='native64')
    parser.add_argument('--check-segment-limit', action='store_true',
                        help='diagnose an invalid DS read instead of entering original startup')
    parser.add_argument('--runtime-objects', type=Path,
                        help='use precompiled harness objects under ROOT/{native64,qemu}')
    args = parser.parse_args()
    if args.backend == 'native64' and (platform.system() != 'Linux' or
                                      platform.machine() not in ('x86_64', 'amd64')):
        parser.error('native64 backend requires an x86-64 Linux host')
    dependencies = ['as', 'ld'] + ([] if args.runtime_objects else ['gcc'])
    if args.backend == 'qemu':
        dependencies.append('qemu-i386')
    for command in dependencies:
        if shutil.which(command) is None:
            parser.error(f'required command unavailable: {command}')
    source = Path(__file__).resolve().parent.parent / 'runtime'
    output = args.output.resolve()
    try:
        manifest, images = extract(args.executable.read_bytes())
        output.mkdir(parents=True, exist_ok=False)
        (output / 'payload.bin').write_bytes(images['payload.bin'])
        (output / 'payload.S').write_text('.section .guest,"awx",@progbits\n'
                                         '.incbin "payload.bin"\n'
                                         '.section .guest_bss,"aw",@nobits\n'
                                         '.zero 0x800000-0x195fb0\n'
                                         '.section .note.GNU-stack,"",@progbits\n')
        native = args.backend == 'native64'
        width = '64' if native else '32'
        compiler = ['gcc', '-m' + width, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-ffreestanding', '-fno-pie', '-fno-pic', '-fno-stack-protector',
                    '-fno-asynchronous-unwind-tables', '-fno-builtin']
        if native:
            compiler += ['-D_GNU_SOURCE', '-DEF_NATIVE64']
        entry = source / ('native64.S' if native else 'entry.S')
        linker = source / ('native64.ld' if native else 'startup.ld')
        assembler = ['as', '--' + width]
        if args.check_segment_limit:
            assembler += ['--defsym', 'CHECK_SEGMENT_LIMIT=1']
        commands = [
            compiler + ['-c', str(source / 'startup.c'), '-o', 'startup.o'],
            compiler + ['-c', str(source / 'x32.c'), '-o', 'x32.o'],
            assembler + [str(entry), '-o', 'entry.o'],
            ['as', '--' + width, 'payload.S', '-o', 'payload.o'],
            ['ld', '-m', 'elf_x86_64' if native else 'elf_i386', '-T', str(linker),
             'entry.o', 'startup.o', 'x32.o', 'payload.o', '-o', 'startup.elf'],
        ]
        if args.runtime_objects:
            objects = args.runtime_objects.resolve() / args.backend
            for name in ('startup.o', 'x32.o'):
                shutil.copyfile(objects / name, output / name)
            entry_object = 'entry-limit.o' if args.check_segment_limit else 'entry.o'
            shutil.copyfile(objects / entry_object, output / 'entry.o')
            # Only payload assembly and linking depend on the external game.
            commands = commands[-2:]
        with (output / 'build.log').open('w') as log:
            for command in commands:
                subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
                               timeout=30, check=True)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        command = [str(output / 'startup.elf')] if native else [
            'qemu-i386', '-B', '0x40000000', '-d', 'in_asm', '-D', 'instructions.log', 'startup.elf']
        with (output / 'calls.log').open('w') as log:
            run = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
                                 timeout=15, check=False)
        report = {
            'source_sha256': manifest['source_sha256'],
            'backend': args.backend,
            'host': platform.platform(),
            'runtime_source_hashes': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in sorted(source.iterdir()) if p.is_file()},
            'returncode': run.returncode,
            'status': {20: 'unsupported_interface', 21: 'unexpected_guest_fault',
                       22: 'host_abi_setup_failure', 23: 'service_limit',
                       24: 'startup_checkpoint_failure', 25: 'allocation_model_limit'}.get(run.returncode, 'unexpected_exit'),
            'command': command,
            'memory_model': 'synthetic 8 MiB image, stack offset 0x7ff000',
            'image_linear_base': 0x10000000 if native else 0,
            'limitations': ['Not playable; stops at unsupported interfaces.',
                            'Synthetic addresses/selectors are not a reference DOS snapshot.',
                            'Only observed startup requests are supported, not their complete APIs.',
                            'X-32 3509 uses the non-DPMI path with a synthetic allocation state.'],
        }
        report.update(analyze_calls((output / 'calls.log').read_text()))
        report['segment_limit_diagnostic'] = args.check_segment_limit
        if args.check_segment_limit:
            report['segment_limit_enforced'] = (
                run.returncode == 21 and len(report['faults']) == 1 and
                report['faults'][0].get('TRAP') == 13 and
                0x800000 <= report['faults'][0].get('IP', 0) < 0x801000)
            report['status'] = ('segment_limit_enforced' if report['segment_limit_enforced']
                                else 'segment_limit_not_enforced')
        if not native:
            report['qemu_version'] = subprocess.run(['qemu-i386', '--version'], capture_output=True,
                text=True, timeout=5, check=True).stdout.splitlines()[0]
            report['limitations'].append('Use the segment-limit diagnostic before relying on QEMU segment-fault fidelity.')
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'{report["status"]}; see {output / "calls.log"}')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}; inspect {output}\n')
    if args.check_segment_limit:
        if not report['segment_limit_enforced']:
            parser.exit(1)
    elif run.returncode != 20 or not report['startup_checkpoint_verified']:
        parser.exit(1)


if __name__ == '__main__':
    main()
