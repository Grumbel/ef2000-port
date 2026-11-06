#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Bounded, asset-backed original-vs-EFPATCH1 framebuffer/RAM replay oracle.

Each requested instruction checkpoint starts a fresh replay of the same script.
This avoids taking nonportable runtime snapshots while providing intermediate
framebuffer and selected guest-RAM comparisons. It does not compare full RAM.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

try:
    from .efpatch import decode as decode_patch
    from .highscores_differential import load_image
except ImportError:
    from efpatch import decode as decode_patch
    from highscores_differential import load_image


SCHEMA = 'ef2000-golden-replay-v1'
STATUS = re.compile(r'^status=(\w+) interrupts=(\d+) ticks=(\d+) irq0=(\d+) ip=0x([0-9a-f]+)\b', re.M)
PATCH_HIT = re.compile(r'^patch_hit: addr=0x([0-9a-f]+) count=(\d+)$', re.M)
PATCH_TOTAL = re.compile(r'^patch_hits: total=(\d+)$', re.M)
GUEST_HASH = re.compile(r'^guest_hash: addr=0x([0-9a-f]+) length=(\d+) sha256=([0-9a-f]{64})$', re.M)
EVENT_ARITY = {'--mouse-event': 4, '--key-event': 4, '--joystick-event': (4, 6)}
MAX_EVENTS = 128
MAX_CASES = 32
MAX_CHECKPOINTS = 8
MAX_TOTAL_POINTS = 64
GUEST_SIZE = 0x4000000


def validate_manifest(manifest, patch_addresses, patch_intervals=()):
    if not isinstance(manifest, dict) or manifest.get('schema') != SCHEMA:
        raise ValueError(f'manifest requires schema {SCHEMA}')
    cases = manifest.get('cases')
    if not isinstance(cases, list) or not 0 < len(cases) <= MAX_CASES:
        raise ValueError(f'manifest needs 1..{MAX_CASES} cases')
    result = []
    names = set()
    total_points = 0
    for case in cases:
        if not isinstance(case, dict):
            raise ValueError('each case must be an object')
        name = case.get('name')
        if (not isinstance(name, str) or not re.fullmatch(r'[a-zA-Z0-9_-]{1,64}', name)
                or name in names):
            raise ValueError('case names must be unique simple filenames (1..64 characters)')
        names.add(name)
        instructions = case.get('instructions')
        timer = case.get('timer_instructions', 23866)
        if (type(instructions) is not int or not 1000 <= instructions <= 2_000_000_000
                or type(timer) is not int or not 1 <= timer <= 1_000_000):
            raise ValueError(f'{name}: invalid instruction limit or timer interval')
        checkpoints = case.get('checkpoints', [])
        if (not isinstance(checkpoints, list) or len(checkpoints) > MAX_CHECKPOINTS or
                any(type(point) is not int or not 1000 <= point < instructions
                    for point in checkpoints) or
                checkpoints != sorted(set(checkpoints))):
            raise ValueError(f'{name}: checkpoints must be ascending, unique instruction '
                             f'limits below {instructions} (at most {MAX_CHECKPOINTS})')
        total_points += len(checkpoints) + 1
        if total_points > MAX_TOTAL_POINTS:
            raise ValueError(f'too many replay checkpoints across cases (max {MAX_TOTAL_POINTS})')
        events = case.get('events', [])
        if not isinstance(events, list) or len(events) > MAX_EVENTS:
            raise ValueError(f'{name}: expected at most {MAX_EVENTS} events')
        arguments = []
        joystick_axes = None
        for event in events:
            if not isinstance(event, list) or len(event) != 2 or event[0] not in EVENT_ARITY:
                raise ValueError(f'{name}: event must be [--mouse/key/joystick-event, value]')
            flag, value = event
            parts = value.split(':') if isinstance(value, str) else []
            if (len(parts) not in (EVENT_ARITY[flag] if isinstance(EVENT_ARITY[flag], tuple)
                                 else (EVENT_ARITY[flag],)) or
                    any(not re.fullmatch(r'-?\d+', part) for part in parts)):
                raise ValueError(f'{name}: invalid {flag} event')
            numbers = [int(part) for part in parts]
            if numbers[0] < 0:
                raise ValueError(f'{name}: negative event tick')
            if numbers[0] > (instructions + timer - 1) // timer:
                raise ValueError(f'{name}: event scheduled after replay tick horizon')
            if flag == '--mouse-event' and not (0 <= numbers[1] < 640 and
                    0 <= numbers[2] < 480 and 0 <= numbers[3] <= 7):
                raise ValueError(f'{name}: invalid mouse coordinates or buttons')
            if flag == '--key-event' and not (0 <= numbers[1] <= 255 and
                    0 <= numbers[2] <= 0x10ffff and numbers[3] in (0, 1)):
                raise ValueError(f'{name}: invalid key scancode, character or state')
            if flag == '--joystick-event':
                axes = len(numbers) - 2
                if (not all(-32768 <= axis <= 32767 for axis in numbers[1:-1]) or
                        not 0 <= numbers[-1] <= 15 or
                        (joystick_axes is not None and joystick_axes != axes)):
                    raise ValueError(f'{name}: invalid joystick axes or buttons')
                joystick_axes = axes
            arguments.extend([flag, value])
        must_hit = case.get('must_hit', [])
        if not isinstance(must_hit, list) or not must_hit:
            raise ValueError(f'{name}: must_hit must name at least one patched entry')
        hits = set()
        for address in must_hit:
            if not isinstance(address, str) or not re.fullmatch(r'0x[0-9a-fA-F]{1,8}', address):
                raise ValueError(f'{name}: invalid patched entry address')
            hits.add(int(address, 16))
        if not hits.issubset(patch_addresses):
            raise ValueError(f'{name}: must_hit includes an address absent from EFPATCH1')
        regions = case.get('guest_hashes', [])
        if not isinstance(regions, list) or len(regions) > 32:
            raise ValueError(f'{name}: guest_hashes must contain at most 32 regions')
        hashes = []
        for region in regions:
            if not isinstance(region, dict) or set(region) != {'address', 'length'}:
                raise ValueError(f'{name}: guest hash requires address and length')
            address, length = region['address'], region['length']
            if (not isinstance(address, str) or not re.fullmatch(r'0x[0-9a-fA-F]{1,8}', address)
                    or type(length) is not int or not 0 < length <= 0x1000000):
                raise ValueError(f'{name}: invalid guest hash address or length')
            start = int(address, 16)
            if start >= 0x4000000 or length > 0x4000000 - start:
                raise ValueError(f'{name}: guest hash outside mapped image')
            if any(start < end and addr < start + length for addr, end in patch_intervals):
                raise ValueError(f'{name}: guest hash overlaps replacement code')
            hashes.append((start, length))
        hashes.sort()
        if (len(set(hashes)) != len(hashes) or
                any(a + length > b for (a, length), (b, _) in zip(hashes, hashes[1:])) or
                sum(length for _, length in hashes) > 0x2000000):
            raise ValueError(f'{name}: duplicate/overlapping or oversized guest hashes')
        allow_no_framebuffer = case.get('allow_no_framebuffer', False)
        if type(allow_no_framebuffer) is not bool or (allow_no_framebuffer and not hashes):
            raise ValueError(f'{name}: allow_no_framebuffer requires guest hashes and a boolean')
        result.append({'name': name, 'instructions': instructions,
                       'timer_instructions': timer, 'arguments': arguments,
                       'must_hit': sorted(hits), 'guest_hashes': hashes,
                       'checkpoints': checkpoints,
                       'allow_no_framebuffer': allow_no_framebuffer})
    return result


def parse_output(output, expected_patches, expected_hashes=()):
    statuses = list(STATUS.finditer(output))
    if len(statuses) != 1:
        raise ValueError('runner must emit exactly one status line')
    match = statuses[0]
    status = {'status': match[1], 'interrupts': int(match[2]),
              'ticks': int(match[3]), 'irq0': int(match[4]), 'ip': int(match[5], 16)}
    hits = {}
    for hit in PATCH_HIT.finditer(output):
        address = int(hit[1], 16)
        if address in hits:
            raise ValueError(f'duplicate patch_hit report at 0x{address:x}')
        hits[address] = int(hit[2])
    totals = PATCH_TOTAL.findall(output)
    if expected_patches:
        if set(hits) != set(expected_patches) or len(totals) != 1:
            raise ValueError('missing or unexpected patch-entry count reports')
        if sum(hits.values()) != int(totals[0]):
            raise ValueError('patch entry counts disagree with reported total')
    elif hits or totals:
        raise ValueError('unpatched runner unexpectedly reported patch hits')
    hashes = {}
    for item in GUEST_HASH.finditer(output):
        region = (int(item[1], 16), int(item[2]))
        if region in hashes:
            raise ValueError(f'duplicate guest_hash report at 0x{region[0]:x}')
        hashes[region] = item[3]
    if set(hashes) != set(expected_hashes):
        raise ValueError('missing or unexpected guest hash reports')
    return status, hits, hashes


def run_one(runner, executable, game_dir, cd_dir, patch_files, case, output, timeout,
            sound_mode):
    output.mkdir(parents=True, exist_ok=False)
    overlay = output / 'drive'
    overlay.mkdir()
    framebuffer = output / 'frame.ppm'
    command = [str(runner), str(executable), '--instruction-limit', str(case['instructions']),
               '--timing', 'instruction', '--timer-instructions', str(case['timer_instructions']),
               '--dos-clock', 'guest', '--vga-clock', 'guest', '--sound', sound_mode, '--controls', 'keyboard',
               '--joystick', 'off', '--game-dir', str(game_dir),
               '--overlay-dir', str(overlay),
               '--framebuffer', str(framebuffer)]
    if cd_dir:
        command.extend(['--cd-dir', str(cd_dir)])
    command.extend(case['arguments'])
    for address, length in case.get('guest_hashes', []):
        command.extend(['--hash-guest', f'0x{address:x}:{length}'])
    for patch in patch_files:
        command.extend(['--patch', str(patch)])
    if patch_files:
        for address in sorted(case.get('must_hit', [])):
            command.extend(['--trace-patch-entry', '0x%x' % address])
    try:
        finished = subprocess.run(command, cwd=output, capture_output=True,
                                  text=True, timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        raise ValueError(f'{case["name"]}: runner timed out after {timeout} seconds') from error
    (output / 'stdout.log').write_text(finished.stdout)
    (output / 'stderr.log').write_text(finished.stderr)
    if finished.returncode:
        raise ValueError(f'{case["name"]}: runner returned {finished.returncode}; see {output}')
    status, hits, hashes = parse_output(finished.stdout, case.get('patch_addresses', set()),
                                       case.get('guest_hashes', []))
    if status['status'] not in ('instruction_limit', 'guest_exit'):
        raise ValueError(f'{case["name"]}: unexpected guest stop: {status["status"]}')
    if not framebuffer.is_file() and not case.get('allow_no_framebuffer', False):
        raise ValueError(f'{case["name"]}: guest framebuffer was not saved')
    return {'status': status, 'patch_hits': {f'0x{k:x}': v for k, v in hits.items()},
            'guest_hashes': {f'0x{k[0]:x}:{k[1]}': v for k, v in hashes.items()},
            'frame_sha256': (hashlib.sha256(framebuffer.read_bytes()).hexdigest()
                             if framebuffer.is_file() else None)}


def differences(baseline, other):
    problems = []
    for key in ('status', 'interrupts', 'ticks', 'irq0'):
        if baseline['status'][key] != other['status'][key]:
            problems.append(f'{key}: {baseline["status"][key]} != {other["status"][key]}')
    if baseline['frame_sha256'] != other['frame_sha256']:
        problems.append('framebuffer SHA256 differs')
    if baseline['guest_hashes'] != other['guest_hashes']:
        for region in sorted(set(baseline['guest_hashes']) | set(other['guest_hashes'])):
            if baseline['guest_hashes'].get(region) != other['guest_hashes'].get(region):
                problems.append(f'guest RAM hash differs at {region}')
    # EIP is intentionally diagnostic only: an instruction-counted boundary
    # may end at different instructions in equivalent functions.
    return problems


def run_suite(runner, executable, game_dir, cd_dir, patches, cases, output,
              patch_addresses, timeout=300, baseline_repeats=2, sound_mode='off'):
    if baseline_repeats not in (1, 2):
        raise ValueError('baseline repeats must be 1 or 2')
    output.mkdir(parents=True, exist_ok=False)
    summary = {'schema': SCHEMA, 'sound_mode': sound_mode,
               'checks': [], 'passed': False,
               'limitations': ['A framebuffer comparison is not a full guest-RAM comparison.',
                               'A patch-entry hit is not proof of correct function behavior.',
                               'Each checkpoint replays from guest entry; it is not a '
                               'snapshot of one continuous run.']}
    if baseline_repeats == 1:
        summary['limitations'].append('One baseline run cannot establish determinism.')
    try:
        for case in cases:
            directory = output / case['name']
            directory.mkdir()
            checks = []
            samples = []
            points = [*case.get('checkpoints', []), case['instructions']]
            for instructions in points:
                last = instructions == case['instructions']
                sample_dir = (directory / f'at-{instructions}'
                              if len(points) > 1 else directory)
                sample_case = dict(case, instructions=instructions)
                baseline = run_one(runner, executable, game_dir, cd_dir, [], sample_case,
                                   sample_dir / 'original', timeout, sound_mode)
                failures = []
                if baseline_repeats == 2:
                    second = run_one(runner, executable, game_dir, cd_dir, [], sample_case,
                                     sample_dir / 'repeat-original', timeout, sound_mode)
                    failures.extend('baseline nondeterminism: ' + issue
                                    for issue in differences(baseline, second))
                patched_case = dict(sample_case,
                                    patch_addresses=set(case['must_hit']))
                patched = run_one(runner, executable, game_dir, cd_dir, patches, patched_case,
                                  sample_dir / 'patched', timeout, sound_mode)
                failures.extend(differences(baseline, patched))
                if not last and baseline['status']['status'] != 'instruction_limit':
                    failures.append('guest stopped before requested intermediate checkpoint')
                if last:
                    for address in case['must_hit']:
                        if not patched['patch_hits'].get(f'0x{address:x}', 0):
                            failures.append(f'patched entry 0x{address:x} not executed')
                samples.append({'instructions': instructions, 'baseline': baseline,
                                'patched': patched, 'failures': failures,
                                'passed': not failures})
                prefix = f'at {instructions} instructions: ' if len(points) > 1 else ''
                checks.extend(prefix + failure for failure in failures)
            summary['checks'].append({'name': case['name'], 'baseline': baseline,
                                      'patched': patched, 'failures': checks,
                                      'checkpoints': samples, 'passed': not checks})
        summary['passed'] = all(check['passed'] for check in summary['checks'])
    except (OSError, ValueError) as error:
        summary['error'] = str(error)
    finally:
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True, help='SHA-pinned _EF2000.EXE')
    parser.add_argument('--runner', type=Path, required=True,
                        help='native ef2000-unicorn-cpp executable')
    parser.add_argument('--game-dir', type=Path, help='default: directory of executable')
    parser.add_argument('--cd-dir', type=Path, help='optional CD data directory')
    parser.add_argument('--patch', type=Path, action='append', required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True,
                        help='new result directory; never overwritten')
    parser.add_argument('--timeout', type=int, default=300, help='seconds per run')
    parser.add_argument('--sound', choices=('off', 'sb16-probe', 'sb16-dma'),
                        default='off', help='runner sound mode (default: off)')
    parser.add_argument('--baseline-repeats', type=int, choices=(1, 2), default=2,
                        help='default 2; use 1 only for faster diagnostics')
    args = parser.parse_args(argv)
    try:
        if args.timeout <= 0 or not args.runner.is_file():
            raise ValueError('runner must exist and timeout must be positive')
        if not args.executable.is_file():
            raise ValueError('original executable does not exist')
        original_hash = hashlib.sha256(args.executable.read_bytes()).hexdigest()
        original = load_image(executable=args.executable)
        patches = [patch.resolve() for patch in args.patch]
        if len(set(patches)) != len(patches):
            raise ValueError('duplicate patch files')
        addresses = []
        intervals = []
        for patch in patches:
            for address, data in decode_patch(patch.read_bytes(), original, GUEST_SIZE):
                addresses.append(address)
                intervals.append((address, address + len(data)))
        intervals.sort()
        if any(a[1] > b[0] for a, b in zip(intervals, intervals[1:])):
            raise ValueError('patches contain overlapping guest writes')
        if not addresses:
            raise ValueError('no patch entry addresses supplied')
        cases = validate_manifest(json.loads(args.manifest.read_text()),
                                  set(addresses), intervals)
        if args.output.exists():
            raise ValueError('output directory already exists (refusing overwrite)')
        game_dir = (args.game_dir or args.executable.parent).resolve()
        if not game_dir.is_dir():
            raise ValueError('game directory does not exist')
        if args.cd_dir and not args.cd_dir.is_dir():
            raise ValueError('CD data directory does not exist')
        output = args.output.resolve()
        # Refuse any output nested under the source game or CD directories.
        # The original game assets must remain read-only inputs to this suite.
        roots = [game_dir, args.executable.resolve().parent]
        if args.cd_dir:
            roots.append(args.cd_dir.resolve())
        if any(output == root or root in output.parents for root in roots):
            raise ValueError('output must be outside the original game/CD directories')
        summary = run_suite(args.runner.resolve(), args.executable.resolve(), game_dir,
                            args.cd_dir.resolve() if args.cd_dir else None,
                            patches, cases, output, set(addresses), args.timeout,
                            args.baseline_repeats, args.sound)
        if hashlib.sha256(args.executable.read_bytes()).hexdigest() != original_hash:
            raise ValueError('original executable changed during replay')
        print(f'replay suite: {sum(c["passed"] for c in summary["checks"])}'
              f'/{len(cases)} passing; report: {output / "summary.json"}')
        for check in summary['checks']:
            for issue in check['failures']:
                print(f'  {check["name"]}: {issue}')
        if 'error' in summary:
            print(f'  {summary["error"]}')
        return 0 if summary['passed'] else 1
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    raise SystemExit(main())
