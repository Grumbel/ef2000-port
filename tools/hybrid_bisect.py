#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Bisect hybrid redirects using the asset-backed replay oracle.

Each trial relinks the complete object set with a subset of guest entries
demoted to their original bodies. Event-free Unicorn replays run headlessly;
cases that inject input use a private Xvfb server. Artifacts and reports
belong under ignored local/.
"""

import argparse
import json
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import sys

try:
    from . import hybrid_link, replay_suite
    from .efpatch import decode as decode_patch
    from .highscores_differential import load_image
except ImportError:
    import hybrid_link
    import replay_suite
    from efpatch import decode as decode_patch
    from highscores_differential import load_image


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_COMPILER_ARTIFACTS = ROOT / 'local/mechanical'


def split(values, count):
    """Partition an ordered list into count nonempty, near-equal slices."""
    quotient, remainder = divmod(len(values), count)
    result, start = [], 0
    for index in range(count):
        size = quotient + (index < remainder)
        if size:
            result.append(values[start:start + size])
        start += size
    return result


def failure_signature(result):
    """Identify a replay failure independently of timing-only shifts.

    Faulting instruction and access details are stable across unrelated
    demotions even when interrupt and tick counters move. For non-fault stops,
    retain the observable output hashes so same-stop visual or RAM divergence
    remains a distinct target.
    """
    if result is None:
        return None
    status = result['status']
    fault = status.get('fault')
    if fault is not None:
        return (status['status'], status['ip'], fault['type'],
                fault['address'], fault['size'], fault['code'])
    return (status['status'], status['ip'], result.get('frame_sha256'),
            tuple(sorted(result.get('guest_hashes', {}).items())))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, default=ROOT / 'local/initial-image/payload.bin')
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--cd-dir', type=Path)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--case', default='main-menu', help='manifest case to use as oracle')
    parser.add_argument('--output', type=Path, required=True,
                        help='new ignored local/ directory; never overwritten')
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--exclude-list', type=Path,
                        default=ROOT / 'decomp/symbols/hybrid-exclude.tsv',
                        help='persistent TSV of entries to keep original')
    parser.add_argument('--keep', action='append', default=[],
                        help='entry that must remain patched and hit (repeatable, default 0xe7835)')
    parser.add_argument('--trace-address', action='append', default=[],
                        help='guest address whose execution count defines the target symptom')
    parser.add_argument('--exclude-address', action='append', default=[],
                        help='keep an already-demoted entry original (ADDR[:REASON])')
    parser.add_argument('--hand-objects', type=Path, action='append', default=None)
    parser.add_argument('--mechanical-dir', type=Path, default=DEFAULT_COMPILER_ARTIFACTS)
    parser.add_argument('--mechanical-report', type=Path)
    parser.add_argument('--functions', type=Path, default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--globals', type=Path, default=ROOT / 'decomp/symbols/globals.tsv')
    parser.add_argument('--abi-header', type=Path, default=hybrid_link.ABI_HEADER)
    parser.add_argument('--max-rounds', type=int, default=16)
    args = parser.parse_args(argv)

    try:
        if args.timeout <= 0 or args.max_rounds <= 0:
            raise ValueError('timeout and max-rounds must be positive')
        for path, label in ((args.image, 'payload'), (args.executable, 'executable'),
                            (args.runner, 'runner'), (args.manifest, 'manifest')):
            if not path.is_file():
                raise ValueError(f'{label} does not exist: {path}')
        output = args.output.resolve()
        if output.exists():
            raise ValueError('output already exists (refusing overwrite)')
        output.mkdir(parents=True)
        game_dir = (args.game_dir or args.executable.parent).resolve()
        cd_dir = args.cd_dir.resolve() if args.cd_dir else None
        if not game_dir.is_dir() or (cd_dir and not cd_dir.is_dir()):
            raise ValueError('game-dir or cd-dir does not exist')
        if output == game_dir or game_dir in output.parents or output == args.executable.parent.resolve():
            raise ValueError('output must stay outside original game data')

        objects, selected = hybrid_link.load_objects(
            args.hand_objects or [ROOT / 'local/hybrid/obj'], args.mechanical_dir,
            args.mechanical_report or args.mechanical_dir / 'report.tsv')
        functions = hybrid_link.read_tsv(args.functions)
        globals_ = hybrid_link.read_tsv(args.globals)
        signatures = hybrid_link.read_tsv(ROOT / 'decomp/symbols/signatures.tsv')
        source = args.image.read_bytes()
        aliases = hybrid_link.load_abi_aliases(args.abi_header)
        persistent_excludes = hybrid_link.load_exclusions(args.exclude_list, len(source))
        for item in args.exclude_address:
            address_text, separator, reason = item.partition(':')
            address = int(address_text, 0)
            if address < 0 or address >= len(source):
                raise ValueError(f'excluded guest address outside image: {address_text}')
            persistent_excludes[address] = reason if separator and reason else \
                'explicitly demoted before bisection'
        initial_blob, _ = hybrid_link.link(objects, functions, globals_, selected,
                                           source, aliases,
                                           explicit_excludes=persistent_excludes,
                                           signatures=signatures)
        initial_writes = decode_patch(initial_blob, source, hybrid_link.GUEST_SIZE)
        initial_addresses = {address for address, _ in initial_writes
                             if address < len(source)}

        keep = {int(value, 0) for value in (args.keep or ['0xe7835'])}
        manifest = json.loads(args.manifest.read_text())
        manifest_case = next((item for item in manifest.get('cases', [])
                              if item.get('name') == args.case), None)
        if manifest_case is None:
            raise ValueError(f'manifest has no case named {args.case!r}')
        # Keep only the requested canaries in this case. During resumed or
        # focused bisection, other original must_hit entries may already be
        # deliberately demoted and must not invalidate the reduced patch set.
        manifest_case['must_hit'] = ['0x%x' % address for address in sorted(keep)]
        cases = replay_suite.validate_manifest(manifest, initial_addresses, ())
        case = next((item for item in cases if item['name'] == args.case), None)
        if case is None:
            raise ValueError(f'manifest has no case named {args.case!r}')

        trace_addresses = sorted({int(value, 0) for value in args.trace_address})
        case['trace_addresses'] = trace_addresses

        replay_runner = args.runner.resolve()
        if case['arguments']:
            xvfb = shutil.which('xvfb-run')
            if not xvfb:
                raise ValueError('xvfb-run is required for replay cases that inject input')
            wrapper = output / 'private-xvfb-runner'
            wrapper.write_text(
                '#!/bin/sh\n'
                'out=$(mktemp) || exit 125\n'
                'err=$(mktemp) || { rm -f "$out"; exit 125; }\n'
                '%s -a %s "$@" >"$out" 2>"$err"\n'
                'result=$?\n'
                'cat "$out"\n'
                'cat "$err" >&2\n'
                'if grep -q "^status=" "$out"; then result=0; fi\n'
                'rm -f "$out" "$err"\n'
                'exit "$result"\n' %
                (shlex.quote(xvfb), shlex.quote(str(args.runner.resolve()))))
            wrapper.chmod(0o755)
            replay_runner = wrapper
        baseline_case = dict(case)
        baseline_case.pop('must_hit', None)
        baseline_case.pop('patch_addresses', None)
        baseline_case['patch_addresses'] = set()
        baseline = replay_suite.run_one(
            replay_runner, args.executable.resolve(), game_dir, cd_dir, [], baseline_case,
            output / 'baseline', args.timeout, 'off')

        trial_log = []
        run_number = 0

        def trial(excluded):
            nonlocal run_number
            run_number += 1
            patch_path = output / f'trial-{run_number:02d}.efpatch'
            excluded_map = dict(persistent_excludes)
            excluded_map.update({address: 'temporary hybrid bisector demotion'
                                 for address in excluded})
            exclude_path = output / f'trial-{run_number:02d}.excluded.tsv'
            with exclude_path.open('w') as stream:
                stream.write('addr\treason\n')
                for address in sorted(excluded):
                    stream.write(f'0x{address:08x}\ttemporary hybrid bisector demotion\n')
            blob, report = hybrid_link.link(
                objects, functions, globals_, selected, source, aliases,
                explicit_excludes=excluded_map, signatures=signatures)
            patch_path.write_bytes(blob)
            writes = decode_patch(blob, source, hybrid_link.GUEST_SIZE)
            addresses = {address for address, _ in writes if address < len(source)}
            must_hit = sorted(keep & addresses)
            if not must_hit:
                raise ValueError('no --keep entry remains patched; choose a valid canary')
            trial_case = dict(case, allow_no_framebuffer=True, must_hit=must_hit,
                              patch_addresses=set(must_hit))
            try:
                result = replay_suite.run_one(
                    replay_runner, args.executable.resolve(), game_dir, cd_dir, [patch_path],
                    trial_case, output / f'trial-{run_number:02d}', args.timeout, 'off')
                failures = replay_suite.differences(baseline, result)
                missing_hits = [address for address in must_hit
                                if result['patch_hits'].get(f'0x{address:x}', 0) == 0]
                failures.extend(f'canary 0x{address:x} was not hit'
                                for address in missing_hits)
            except (OSError, ValueError) as error:
                result = None
                failures = [str(error)]
            passed = not failures
            trial_log.append({'trial': run_number,
                              'excluded_file': exclude_path.name,
                              'excluded_count': len(excluded),
                              'excluded_sha256': hashlib.sha256(
                                  exclude_path.read_bytes()).hexdigest(),
                              'functions_redirected': report['functions_redirected'],
                              'patched_bytes': report['redirect_bytes'],
                              'passed': passed, 'failures': failures,
                              'status': result['status'] if result else None,
                              'failure_signature': failure_signature(result)})
            print(f'trial {run_number}: {report["functions_redirected"]} redirects; '
                  f'{"PASS" if passed else "FAIL"}' +
                  ('' if not failures else ': ' + '; '.join(failures)))
            return passed, result, failures

        def match_signature(result):
            if trace_addresses:
                if result is None:
                    return None
                return tuple(result['patch_hits'].get(f'0x{address:x}', 0)
                             for address in trace_addresses)
            return failure_signature(result)

        initial_pass, initial_result, _ = trial(set())
        if initial_pass:
            raise ValueError('complete hybrid passes this replay case; no culprit to bisect')
        target_signature = match_signature(initial_result)
        if target_signature is None:
            raise ValueError('initial hybrid failure has no parsed target signature')
        if trace_addresses and not any(target_signature):
            raise ValueError('full hybrid does not hit any requested trace address')

        current = sorted(initial_addresses - keep)
        excluded = set()
        outcome = 'unresolved'
        candidate_sets = []
        granularity = 2
        for _round_number in range(1, args.max_rounds + 1):
            if not current:
                outcome = 'no-patch-entries-required'
                break
            chunks = split(current, min(granularity, len(current)))
            reduced = False
            for chunk in chunks:
                chunk = set(chunk)
                _, reduced_result, _ = trial(excluded | chunk)
                if match_signature(reduced_result) == target_signature:
                    excluded |= chunk
                    current = [address for address in current if address not in chunk]
                    granularity = max(2, granularity - 1)
                    reduced = True
                    break
            if reduced:
                continue
            if granularity >= len(current):
                outcome = 'minimal-set'
                break
            granularity = min(len(current), granularity * 2)
        if outcome == 'unresolved' and len(current) < 2:
            outcome = 'minimal-set'
        result = {'schema': 'ef2000-hybrid-bisect-v1', 'case': args.case,
                  'baseline': baseline, 'outcome': outcome,
                  'target_failure_signature': list(target_signature),
                  'trace_addresses': [f'0x{x:08x}' for x in trace_addresses],
                  'initial_redirect_addresses': [f'0x{x:08x}'
                                                for x in sorted(initial_addresses)],
                  'persistent_excludes': [
                      {'address': f'0x{x:08x}', 'reason': persistent_excludes[x]}
                      for x in sorted(persistent_excludes)],
                  'candidate_addresses': [f'0x{x:08x}' for x in current],
                  'candidate_sets': [[f'0x{x:08x}' for x in group]
                                     for group in candidate_sets],
                  'excluded_addresses': [f'0x{x:08x}' for x in sorted(excluded)],
                  'trials': trial_log}
        (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        candidate_summary = ','.join(result['candidate_addresses']) or (
            ','.join(str(len(group)) for group in candidate_sets) + ' addresses in each candidate set'
            if candidate_sets else 'none')
        print(f'bisect: {outcome}; candidates={candidate_summary}; '
              f'report={output / "report.json"}')
        return 0 if outcome == 'minimal-set' else 2
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    sys.exit(main())
