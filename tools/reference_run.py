#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run DOSBox-X against a disposable game copy and retain experiment metadata."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

try:
    from .extract_image import extract
except ImportError:
    from extract_image import extract


def instrument_entry(executable, output, probe='x32_services.S'):
    original = executable.read_bytes()
    manifest, _ = extract(original)  # Require the known binary before patching.
    source = Path(__file__).resolve().parent.parent / 'probes' / probe
    subprocess.run(['as', '--32', str(source), '-o', str(output / 'hook.o')],
                   check=True, timeout=10)
    subprocess.run(['objcopy', '-O', 'binary', '--only-section=.text',
                    str(output / 'hook.o'), str(output / 'hook.bin')],
                   check=True, timeout=10)
    hook = (output / 'hook.bin').read_bytes()
    if len(hook) > 0x600:
        raise ValueError('reference hook exceeds its reserved application range')
    patched = bytearray(original)
    start = manifest['layout']['payload_file_offset']
    patched[start:start + len(hook)] = hook
    executable.write_bytes(patched)
    return {'source_sha256': hashlib.sha256(original).hexdigest(),
            'patched_sha256': hashlib.sha256(patched).hexdigest(),
            'hook_source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'payload_patch_range': [0, len(hook)]}


def fingerprints(directory):
    return {str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.rglob('*')) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True,
                        help='new experiment directory; existing paths are refused')
    parser.add_argument('--emulator', default='dosbox-x')
    parser.add_argument('--seconds', type=int, default=15, choices=range(1, 61),
                        metavar='1..60')
    parser.add_argument('--headless', action='store_true',
                        help='use SDL dummy video/audio; no visual verification')
    probes = parser.add_mutually_exclusive_group()
    probes.add_argument('--entry-probe', action='store_true',
                        help='instrument the COPY to record original X-32 services and exit')
    probes.add_argument('--time-probe', action='store_true',
                        help='instrument the COPY to record DOS time queries and exit')
    probes.add_argument('--vector-probe', action='store_true',
                        help='instrument the COPY to measure X-32 interrupt vectors and exit')
    probes.add_argument('--memory-probe', action='store_true',
                        help='instrument the COPY to measure DOS conventional allocation and exit')
    probes.add_argument('--file-probe', action='store_true',
                        help='instrument the COPY to measure DOS file reads/seeks and exit')
    probes.add_argument('--exception-probe', action='store_true',
                        help='instrument the COPY to measure an X-32 divide exception frame')
    probes.add_argument('--sb16-probe', action='store_true',
                        help='instrument the COPY to measure DSP reset/version and SB16 resources (no playback)')
    args = parser.parse_args()
    source = args.game_dir.resolve()
    output = args.output.resolve()
    emulator = shutil.which(args.emulator)
    if emulator is None:
        parser.error(f'emulator not found: {args.emulator}')
    if not (source / '_EF2000.EXE').is_file():
        parser.error('--game-dir must contain _EF2000.EXE')
    if source == output or source in output.parents:
        parser.error('--output must be outside the source game directory')
    if output.exists():
        parser.error('--output must not already exist')
    before = fingerprints(source)
    output.mkdir(parents=True)
    game_copy = output / 'drive' / 'EF2000'
    shutil.copytree(source, game_copy)
    instrumentation = None
    if args.entry_probe or args.time_probe or args.vector_probe or args.memory_probe or args.file_probe or args.exception_probe or args.sb16_probe:
        instrumentation = instrument_entry(game_copy / '_EF2000.EXE', output,
            'sb16_dsp.S' if args.sb16_probe else
            'x32_exception.S' if args.exception_probe else
            'dos_files.S' if args.file_probe else
            'dos_memory.S' if args.memory_probe else
            'x32_vectors.S' if args.vector_probe else
            'dos_time.S' if args.time_probe else 'x32_services.S')
    config = '''[sdl]
fullscreen=false
output=surface
waitonerror=false
[dosbox]
memsize=32
[cpu]
core=normal
cycles=fixed 60000
[log]
logfile=dosbox-x.log
int21=true
fileio=true
[midi]
mididevice=none
[autoexec]
mount c drive
c:
cd EF2000
set DID=.
set quick_flag=1
_ef2000.exe gui
exit
'''
    if args.sb16_probe:
        config = config.replace('[autoexec]', '[sblaster]\nsbtype=sb16\nsbbase=220\nirq=7\ndma=1\nhdma=5\n[autoexec]')
    (output / 'dosbox-x.conf').write_text(config)
    environment = os.environ.copy()
    if args.headless:
        environment.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    command = [emulator, '-conf', 'dosbox-x.conf', '-fastlaunch',
               '-time-limit', str(args.seconds), '-nomenu']
    timed_out = False
    with (output / 'console.log').open('w') as console:
        try:
            result = subprocess.run(command, cwd=output, env=environment,
                                    stdout=console, stderr=subprocess.STDOUT,
                                    timeout=args.seconds + 10, check=False)
            returncode = result.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
            returncode = None
    after = fingerprints(game_copy)
    original_after = fingerprints(source)
    summary = {
        'command': command,
        'headless': args.headless,
        'time_limit_seconds': args.seconds,
        'host_timeout': timed_out,
        'emulator_returncode': returncode,
        'entry_instrumentation': instrumentation,
        'source_file_hashes': before,
        'source_unchanged': before == original_after,
        'copy_changed_or_added': sorted(key for key in after if before.get(key) != after[key]),
        'copy_deleted': sorted(before.keys() - after.keys()),
        'limitations': [
            'Process exit status does not establish game startup or menu success.',
            'INT 21h/file I/O logging is requested, but depends on emulator build support.',
            'No instruction trace or protected-mode memory snapshot is captured.',
        ],
    }
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Reference experiment saved in {output}')
    print(f'Emulator return code: {returncode}; source unchanged: {summary["source_unchanged"]}')
    if timed_out or returncode != 0 or not summary['source_unchanged']:
        parser.exit(1)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from None
