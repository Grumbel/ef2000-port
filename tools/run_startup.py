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
    clock_samples = []
    file_requests = []
    console = []
    port_io = []
    pit = []
    real_mode_requests = []
    guest_exits = []
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
        elif line.startswith('CLOCK '):
            clock_samples.append(values)
        elif line.startswith('FILE OPEN '):
            file_requests.append(line[len('FILE OPEN '):])
        elif line.startswith('CONSOLE '):
            console.append(line[len('CONSOLE '):])
        elif line.startswith('IO '):
            port_io.append(values)
        elif line.startswith('PIT '):
            pit.append(values)
        elif line.startswith('REALMODE '):
            real_mode_requests.append(values)
        elif line.startswith('EXIT '):
            guest_exits.append(values)
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
            'clock_samples': clock_samples,
            'file_requests': file_requests, 'console': console, 'port_io': port_io, 'pit': pit,
            'real_mode_requests': real_mode_requests,
            'guest_exits': guest_exits,
            'checkpoint': checkpoint, 'allocation': allocation, 'startup_checkpoint_verified': verified}


def main():
    parser = argparse.ArgumentParser(
        description='Run original EF2000 code on Linux (main-menu port; audio disabled by default).',
        epilog='Example: %(prog)s /path/to/_EF2000.EXE --output local/startup-01')
    version_file = Path(__file__).resolve().parent.parent / 'VERSION'
    version = os.environ.get('EF2000_VERSION') or version_file.read_text().strip()
    parser.add_argument('--version', action='version', version=f'ef2000 {version}')
    parser.add_argument('executable', type=Path,
                        help='path to the original DOS _EF2000.EXE')
    parser.add_argument('--output', type=Path, required=True, metavar='DIR',
                        help='create DIR for the extracted game payload, compiled startup '
                             'harness, logs, report.json, and framebuffer.png; '
                             'DIR/drive holds local game-file changes '
                             '(required; must not already exist; e.g. local/startup-01)')
    parser.add_argument('--backend', choices=['native64', 'qemu'], default='native64',
                        help='execution backend: native64 requires x86-64 Linux; '
                             'qemu uses qemu-i386 (default: %(default)s)')
    parser.add_argument('--real-mode', choices=['stop', 'unicorn'], default='unicorn',
                        help='real-mode driver execution (default: %(default)s)')
    parser.add_argument('--sound', choices=['off', 'probe'], default='off',
                        help='sound configuration: off disables audio locally; probe runs '
                             'original driver detection (default: %(default)s)')
    parser.add_argument('--no-sound', action='store_const', const='off', dest='sound',
                        help='disable music, sound effects, and speech in a local configuration copy')
    parser.add_argument('--controls', choices=['auto', 'keyboard', 'joystick', 'original'], default='auto',
                        help='flight controls in the local config: auto selects joystick when connected, '
                             'otherwise keyboard (default: %(default)s)')
    parser.add_argument('--joystick', default='auto', metavar='auto|off|N',
                        help='SDL joystick device index, auto uses the first connected device (default: auto)')
    parser.add_argument('--joystick-axes', default='0,1,2,3', metavar='X,Y,Z,R',
                        help='2 to 4 SDL axis indices; prefix an index with - to invert it (default: %(default)s)')
    parser.add_argument('--headless', action='store_true', help='run without an SDL window; save framebuffer.png on exit')
    parser.add_argument('--seconds', type=int, default=0,
                        help='stop after N seconds, saving the framebuffer (default: 0, no limit)')
    parser.add_argument('--check-segment-limit', action='store_true',
                        help='diagnose an invalid DS read instead of entering original startup')
    parser.add_argument('--runtime-objects', type=Path, metavar='ROOT',
                        help='use precompiled harness objects under ROOT/{native64,qemu} '
                             'instead of compiling them with GCC')
    args = parser.parse_args()
    if args.joystick not in ('auto', 'off') and not args.joystick.isdecimal():
        parser.error('--joystick must be auto, off, or a nonnegative device index')
    axis_tokens = args.joystick_axes.split(',')
    if not 2 <= len(axis_tokens) <= 4 or any(not t.lstrip('-').isdecimal() or t.startswith('--') for t in axis_tokens):
        parser.error('--joystick-axes requires 2 to 4 comma-separated axis indices, optionally inverted with -')
    joystick_axes = [(int(t.lstrip('-')), -1 if t.startswith('-') else 1) for t in axis_tokens]
    if args.seconds < 0:
        parser.error('--seconds must be nonnegative')
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
        overlay = output / 'drive' / 'EF2000'
        overlay.mkdir(parents=True)
        if args.sound == 'off' or args.controls != 'original':
            config = args.executable.resolve().parent / 'EF2000.CFG'
            lines = config.read_bytes().splitlines()
            settings = {b'MUSIC': b'0,127', b'SOUNDFX': b'0,127', b'SPEECH': b'0'} if args.sound == 'off' else {}
            if args.controls != 'original':
                settings[b'CONTROL'] = b'1' if args.controls == 'joystick' else b'0'
            lines = [line.split(b'=', 1)[0] + b'=' + settings[line.split(b'=', 1)[0]]
                     if line.split(b'=', 1)[0] in settings else line for line in lines]
            (overlay / 'EF2000.CFG').write_bytes(b'\r\n'.join(lines) + b'\r\n')
        for directory in args.executable.resolve().parent.rglob('*'):
            if directory.is_dir():
                (overlay / directory.relative_to(args.executable.resolve().parent)).mkdir(parents=True, exist_ok=True)
        (output / 'payload.bin').write_bytes(images['payload.bin'])
        (output / 'payload.S').write_text('.section .guest,"awx",@progbits\n'
                                         '.incbin "payload.bin"\n'
                                         '.section .guest_bss,"aw",@nobits\n'
                                         '.zero 0x4000000-0x195fb0\n'
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
        assembler = ['as', '--' + width, '-I', str(source)]
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
        command.append(str(args.executable.resolve().parent))
        real_mode_requests = []
        with (output / 'calls.log').open('w') as log:
            if args.real_mode == 'unicorn' and not args.check_segment_limit:
                try:
                    from .real_mode import run as run_real_mode
                except ImportError:
                    from real_mode import run as run_real_mode
                run, real_mode_requests = run_real_mode(command, output, log, timeout=args.seconds,
                                                       headless=args.headless, controls=args.controls,
                                                       joystick=args.joystick, joystick_axes=joystick_axes)
            else:
                run = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=args.seconds or None, check=False)
        report = {
            'source_sha256': manifest['source_sha256'],
            'backend': args.backend,
            'host': platform.platform(),
            'runtime_source_hashes': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in sorted(source.iterdir()) if p.is_file()},
            'returncode': run.returncode,
            'status': {20: 'unsupported_interface', 21: 'unexpected_guest_fault',
                       22: 'host_abi_setup_failure', 23: 'service_limit',
                       24: 'startup_checkpoint_failure', 25: 'allocation_model_limit',
                       26: 'guest_exit', 27: 'time_limit', 28: 'window_closed'}.get(run.returncode, 'unexpected_exit'),
            'command': command,
            'memory_model': 'synthetic 64 MiB image, stack offset 0x3fff000',
            'clock_model': 'host CLOCK_REALTIME in UTC, truncated to hundredths; samples logged',
            'image_linear_base': 0x10000000,
            'dos_window_linear_base': 0x01000000,
            'display': 'headless' if args.headless else 'SDL2',
            'sound': args.sound,
            'controls': args.controls,
            'joystick': args.joystick,
            'joystick_axes': args.joystick_axes,
            'limitations': ['Menu and initial flight input verified; broader gameplay and mission completion remain unverified.',
                            'Synthetic addresses/selectors are not a reference DOS snapshot.',
                            'Only observed startup requests are supported, not their complete APIs.',
                            'X-32 allocation uses a synthetic no-swap state.',
                            'Game assets are read-only; writes use the local drive overlay. Host filenames are uppercase.',
                            'Exception vectors are registered but exception delivery is not implemented.',
                            'PIT IRQ0 is delivered through host signals; masked/coalesced ticks are approximate.',
                            'CLI/STI update a virtual interrupt state; PUSHF/POPF reflection is not implemented.',
                            'CPU alignment checking is disabled on the first AC fault; PUSHF reflection differs from DOS.',
                            'Keyboard IRQ1, mouse, and up to four SDL joystick axes/buttons are connected; physical joystick validation is pending.',
                            'Sound hardware is absent; audio is disabled in a local configuration copy by default.',
                            'VGA/VBE implements the observed indexed banked modes, not the complete hardware API.'],
        }
        report.update(analyze_calls((output / 'calls.log').read_text()))
        report['real_mode_backend'] = args.real_mode
        report['real_mode_execution'] = real_mode_requests
        report['segment_limit_diagnostic'] = args.check_segment_limit
        if args.check_segment_limit:
            report['segment_limit_enforced'] = (
                run.returncode == 21 and len(report['faults']) == 1 and
                report['faults'][0].get('TRAP') == 13 and
                0x4000000 <= report['faults'][0].get('IP', 0) < 0x4001000)
            report['status'] = ('segment_limit_enforced' if report['segment_limit_enforced']
                                else 'segment_limit_not_enforced')
        if not native:
            report['qemu_version'] = subprocess.run(['qemu-i386', '--version'], capture_output=True,
                text=True, timeout=5, check=True).stdout.splitlines()[0]
            report['limitations'].append('Use the segment-limit diagnostic before relying on QEMU segment-fault fidelity.')
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'{report["status"]}; see {output / "calls.log"}')
    except (OSError, ValueError, ImportError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}; inspect {output}\n')
    if args.check_segment_limit:
        if not report['segment_limit_enforced']:
            parser.exit(1)
    else:
        expected_stop = run.returncode in (27, 28) or (run.returncode == 20 and args.real_mode == 'stop')
        if not expected_stop or not report['startup_checkpoint_verified']:
            parser.exit(1)


if __name__ == '__main__':
    main()
