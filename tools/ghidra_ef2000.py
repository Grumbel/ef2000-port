#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Import the pinned EF2000 X-32 image into Ghidra and decompile guest offsets.

All proprietary bytes and Ghidra project files stay in the ignored local/
directory by default. Ghidra is supplied by the user, not vendored here.
"""

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


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / 'tools' / 'ghidra'
PROJECT_NAME = 'EF2000'
IMAGE_NAME = 'payload.bin'
IMAGE_SIZE = 0x195fb0


def headless_path(option):
    if option:
        candidate = Path(option).expanduser()
    elif os.environ.get('GHIDRA_HOME'):
        candidate = Path(os.environ['GHIDRA_HOME']) / 'support' / 'analyzeHeadless'
    else:
        resolved = shutil.which('analyzeHeadless')
        if not resolved:
            raise ValueError('set --analyze-headless or GHIDRA_HOME (Ghidra is not on PATH)')
        candidate = Path(resolved)
    if candidate.is_dir():
        candidate /= 'support/analyzeHeadless'
    if not candidate.is_file():
        raise ValueError(f'Ghidra headless launcher not found: {candidate}')
    return str(candidate.resolve())


def guest_offset(text):
    try:
        value = int(text, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f'invalid guest offset: {text}') from error
    if not 0 <= value < IMAGE_SIZE:
        raise argparse.ArgumentTypeError(f'guest offset outside initialized image: {text}')
    return value


def project_args(workspace, launcher):
    return [launcher, str(workspace / 'projects'), PROJECT_NAME,
            '-scriptPath', str(SCRIPTS)]


def import_image(args):
    # The extractor checks the exact original executable fingerprint. Never
    # import the MZ stub as a flat 32-bit program: its offsets do not match the
    # addresses emitted by Unicorn's block profiler.
    manifest, images = extract(args.executable.read_bytes())
    payload = images[IMAGE_NAME]
    if len(payload) != IMAGE_SIZE:
        raise ValueError('unexpected EF2000 initialized image length')
    workspace = args.workspace.resolve()
    project_dir = workspace / 'projects'
    if (project_dir / (PROJECT_NAME + '.gpr')).exists():
        raise ValueError('Ghidra project already exists; use decompile to reuse it')
    image_path = workspace / IMAGE_NAME
    manifest_path = workspace / 'manifest.json'
    if image_path.exists() and image_path.read_bytes() != payload:
        raise ValueError(f'refusing to replace a different image: {image_path}')
    if manifest_path.exists() and json.loads(manifest_path.read_text()) != manifest:
        raise ValueError(f'refusing to replace a different manifest: {manifest_path}')
    launcher = headless_path(args.analyze_headless)
    project_dir.mkdir(parents=True, exist_ok=True)
    if not image_path.exists():
        image_path.write_bytes(payload)
    if not manifest_path.exists():
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    command = project_args(workspace, launcher) + [
        '-import', str(image_path),
        '-loader', 'BinaryLoader', '-loader-baseAddr', '0',
        '-loader-blockName', 'X32_IMAGE',
        '-processor', 'x86:LE:32:default', '-cspec', 'windows',
        '-preScript', 'EF2000Seed.java',
    ]
    subprocess.run(command, check=True)
    project_path = project_dir / (PROJECT_NAME + '.gpr')
    if not project_path.is_file():
        raise ValueError(f'Ghidra returned without creating its project: {project_path}')
    print(f'Ghidra project: {project_path}')


def decompile(args):
    workspace = args.workspace.resolve()
    manifest_path = workspace / 'manifest.json'
    image_path = workspace / IMAGE_NAME
    project_path = workspace / 'projects' / (PROJECT_NAME + '.gpr')
    if not manifest_path.is_file() or not image_path.is_file() or not project_path.is_file():
        raise ValueError('no imported project; run the import command first')
    manifest = json.loads(manifest_path.read_text())
    if (manifest['protected_image']['initialized_range'] != [0, IMAGE_SIZE] or
            manifest['protected_image']['payload_sha256'] !=
            hashlib.sha256(image_path.read_bytes()).hexdigest()):
        raise ValueError('imported image does not match its extraction manifest')
    launcher = headless_path(args.analyze_headless)
    output = (args.output or workspace / 'decompiled').resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous_times = {
        offset: (output / f'0x{offset:08x}.c').stat().st_mtime_ns
        if (output / f'0x{offset:08x}.c').exists() else -1
        for offset in args.offsets
    }
    command = project_args(workspace, launcher) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000Decompile.java', str(output),
        *(f'{offset:x}' for offset in args.offsets),
    ]
    subprocess.run(command, check=True)
    missing = [offset for offset in args.offsets
               if not (output / f'0x{offset:08x}.c').is_file() or
               (output / f'0x{offset:08x}.c').stat().st_mtime_ns <= previous_times[offset]]
    if missing:
        raise ValueError('Ghidra returned without fresh decompilation for: ' +
                         ', '.join(f'0x{offset:x}' for offset in missing))
    print(f'Decompilation: {output}')


def copy_callers(args):
    """Export direct copy callsites with their post-CALL guest return IPs."""
    workspace = args.workspace.resolve()
    image_path = workspace / IMAGE_NAME
    manifest_path = workspace / 'manifest.json'
    project_path = workspace / 'projects' / (PROJECT_NAME + '.gpr')
    if not image_path.is_file() or not manifest_path.is_file() or not project_path.is_file():
        raise ValueError('no imported project; run the import command first')
    manifest = json.loads(manifest_path.read_text())
    if (manifest['protected_image']['initialized_range'] != [0, IMAGE_SIZE] or
            manifest['protected_image']['payload_sha256'] !=
            hashlib.sha256(image_path.read_bytes()).hexdigest()):
        raise ValueError('imported image does not match its extraction manifest')
    output = (args.output or workspace / 'copy-callers.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000CopyCallers.java', str(output),
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh copy-caller map')
    print(f'Copy callers: {output}')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='command', required=True)
    for name in ('import', 'decompile', 'copy-callers'):
        sub = subparsers.add_parser(name)
        sub.add_argument('--workspace', type=Path, default=ROOT / 'local/ghidra',
                         help='ignored Ghidra project and image workspace')
        sub.add_argument('--analyze-headless', help='Ghidra launcher or installation directory')
    imp = subparsers.choices['import']
    imp.add_argument('executable', type=Path, help='SHA-pinned original _EF2000.EXE')
    dec = subparsers.choices['decompile']
    dec.add_argument('offsets', nargs='+', type=guest_offset, help='guest offsets, e.g. 0xe9a24')
    dec.add_argument('--output', type=Path, help='decompiled C files (default: workspace/decompiled)')
    callers = subparsers.choices['copy-callers']
    callers.add_argument('--output', type=Path, help='TSV output (default: workspace/copy-callers.tsv)')
    args = parser.parse_args(argv)
    try:
        {'import': import_image, 'decompile': decompile, 'copy-callers': copy_callers}[args.command](args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'ghidra_ef2000: {error}\n')


if __name__ == '__main__':
    main()
