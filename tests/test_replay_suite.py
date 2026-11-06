# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free replay suite checks using a deterministic synthetic guest runner."""

import json
from pathlib import Path
import tempfile
import unittest

from tools import replay_suite


PATCHES = {0x20}


def manifest(*, name='startup', hits=None, events=None, limit=10000,
             guest_hashes=None, checkpoints=None, allow_no_framebuffer=False):
    return {'schema': replay_suite.SCHEMA, 'cases': [
        {'name': name, 'instructions': limit, 'timer_instructions': 1000,
         'events': events or [], 'must_hit': hits if hits is not None else ['0x20'],
         'guest_hashes': guest_hashes or [], 'checkpoints': checkpoints or [],
         'allow_no_framebuffer': allow_no_framebuffer} ]}


def fake_runner(path):
    path.write_text('''#!/usr/bin/env python3
import hashlib
import pathlib
import sys
a = sys.argv
if a[a.index('--dos-clock') + 1] != 'guest' or \
        a[a.index('--vga-clock') + 1] != 'guest' or \
        a[a.index('--controls') + 1] != 'keyboard' or \
        a[a.index('--joystick') + 1] != 'off':
    raise SystemExit('replay suite must use the deterministic guest clock and keyboard-only input')
patched = '--patch' in a
budget = int(a[a.index('--instruction-limit') + 1])
output = pathlib.Path(a[a.index('--framebuffer') + 1])
if ('--no-frame' not in a or budget >= 5000) and not (patched and '--patch-no-frame' in a):
    output.write_bytes(b'P6\\n1 1\\n255\\n' + (b'\\x01\\x02\\x03' if '--different-frame' in a else b'\\x00\\x00\\x00'))
print('status=instruction_limit interrupts=19 ticks=12 irq0=10 ip=0x101 ax=0x0')
if patched:
    count = '0' if '--no-hit' in a or ('--late-hit' in a and budget < 5000) else '3'
    print('patch_hit: addr=0x20 count=' + count)
    print('patch_hits: total=' + count)
for i, flag in enumerate(a):
    if flag == '--hash-guest':
        addr, length = a[i + 1].split(':')
        size = int(length)
        data = bytes(size)
        if patched and ('--different-memory' in a or
                ('--early-memory-difference' in a and budget < 5000)):
            data = b'\\x01' + data[1:]
        print(f'guest_hash: addr=0x{int(addr, 0):x} length={size} sha256={hashlib.sha256(data).hexdigest()}')
''')
    path.chmod(0o755)


class ManifestTests(unittest.TestCase):
    def test_accepts_scripted_events_and_known_patch_hit(self):
        data = manifest(events=[['--mouse-event', '4:320:240:1'],
                                ['--key-event', '6:40:32:1'],
                                ['--joystick-event', '9:-128:128:0']])
        cases = replay_suite.validate_manifest(data, PATCHES)
        self.assertEqual(cases[0]['must_hit'], [0x20])
        self.assertEqual(cases[0]['arguments'][:2], ['--mouse-event', '4:320:240:1'])

    def test_rejects_missing_evidence_and_unsafe_event_arguments(self):
        bad = manifest(hits=[])
        with self.assertRaisesRegex(ValueError, 'must_hit'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(hits=['0x21'])
        with self.assertRaisesRegex(ValueError, 'absent from EFPATCH1'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(events=[['--framebuffer', '/tmp/overwrite']])
        with self.assertRaisesRegex(ValueError, 'event must'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(events=[['--mouse-event', '10001:1:2:3']])
        with self.assertRaisesRegex(ValueError, 'after replay tick horizon'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(events=[['--joystick-event', '3:0:0:0'],
                               ['--joystick-event', '5:0:0:0:0:0']])
        with self.assertRaisesRegex(ValueError, 'invalid joystick'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(events=[['--mouse-event', '5:640:200:1']])
        with self.assertRaisesRegex(ValueError, 'mouse coordinates'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(name='../../escape')
        with self.assertRaisesRegex(ValueError, 'filenames'):
            replay_suite.validate_manifest(bad, PATCHES)
        bad = manifest(limit=True)
        with self.assertRaisesRegex(ValueError, 'instruction limit'):
            replay_suite.validate_manifest(bad, PATCHES)

    def test_memory_ranges_fail_closed_before_guest_replay(self):
        region = [{'address': '0x300', 'length': 128}]
        cases = replay_suite.validate_manifest(manifest(guest_hashes=region),
                                                PATCHES, [(0x20, 0x30)])
        self.assertEqual(cases[0]['guest_hashes'], [(0x300, 128)])
        with self.assertRaisesRegex(ValueError, 'overlaps replacement'):
            replay_suite.validate_manifest(manifest(guest_hashes=[
                {'address': '0x28', 'length': 8}]), PATCHES, [(0x20, 0x30)])
        with self.assertRaisesRegex(ValueError, 'overlapping'):
            replay_suite.validate_manifest(manifest(guest_hashes=[
                {'address': '0x300', 'length': 16},
                {'address': '0x308', 'length': 16}]), PATCHES)
        with self.assertRaisesRegex(ValueError, 'outside mapped image'):
            replay_suite.validate_manifest(manifest(guest_hashes=[
                {'address': '0x3ffffff', 'length': 2}]), PATCHES)

    def test_checkpoint_limits_are_validated_across_cases(self):
        data = manifest(checkpoints=[1000, 2500, 5000])
        self.assertEqual(replay_suite.validate_manifest(data, PATCHES)[0]['checkpoints'],
                         [1000, 2500, 5000])
        for invalid in ([5000, 1000], [1000, 1000], [10000], [0], [True],
                        list(range(1000, 10000, 1000))):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ValueError, 'checkpoints'):
                replay_suite.validate_manifest(manifest(checkpoints=invalid), PATCHES)
        many = manifest()
        many['cases'] = [dict(many['cases'][0], name=f'case-{idx}', checkpoints=[1000, 2000])
                         for idx in range(22)]
        with self.assertRaisesRegex(ValueError, 'too many replay checkpoints'):
            replay_suite.validate_manifest(many, PATCHES)

    def test_optional_pre_video_framebuffer_requires_ram_oracle(self):
        with self.assertRaisesRegex(ValueError, 'allow_no_framebuffer'):
            replay_suite.validate_manifest(manifest(allow_no_framebuffer=True), PATCHES)
        data = manifest(allow_no_framebuffer=True,
                        guest_hashes=[{'address': '0x300', 'length': 4}])
        self.assertTrue(replay_suite.validate_manifest(data, PATCHES)[0]['allow_no_framebuffer'])

    def test_fail_closed_patch_hit_parser(self):
        status = 'status=instruction_limit interrupts=3 ticks=4 irq0=5 ip=0xff ax=0x0\n'
        with self.assertRaisesRegex(ValueError, 'count reports'):
            replay_suite.parse_output(status, PATCHES)
        with self.assertRaisesRegex(ValueError, 'unexpectedly'):
            replay_suite.parse_output(status + 'patch_hits: total=0\n', set())
        with self.assertRaisesRegex(ValueError, 'counts disagree'):
            replay_suite.parse_output(status + 'patch_hit: addr=0x20 count=1\n'
                                      'patch_hits: total=2\n', PATCHES)
        with self.assertRaisesRegex(ValueError, 'guest hash reports'):
            replay_suite.parse_output(status, set(), [(0x300, 4)])


class SyntheticReplayTests(unittest.TestCase):
    def setUp(self):
        self.context = tempfile.TemporaryDirectory()
        self.addCleanup(self.context.cleanup)
        self.root = Path(self.context.name)
        self.runner = self.root / 'runner.py'
        fake_runner(self.runner)
        self.executable = self.root / '_EF2000.EXE'
        self.executable.write_bytes(b'synthetic')
        self.patch = self.root / 'test.efpatch'
        self.patch.write_bytes(b'synthetic-patch')

    def run_case(self, case, *, baseline_repeats=1):
        output = self.root / 'result'
        return replay_suite.run_suite(self.runner, self.executable, self.root,
                                      None, [self.patch], [case], output, PATCHES,
                                      timeout=5, baseline_repeats=baseline_repeats)

    def test_identical_replay_and_patch_hit_pass(self):
        case = replay_suite.validate_manifest(manifest(guest_hashes=[
            {'address': '0x300', 'length': 4}]), PATCHES)[0]
        summary = self.run_case(case, baseline_repeats=2)
        self.assertTrue(summary['passed'])
        self.assertEqual(summary['checks'][0]['patched']['patch_hits'], {'0x20': 3})
        self.assertEqual(len(summary['checks'][0]['patched']['guest_hashes']['0x300:4']), 64)
        self.assertTrue((self.root / 'result/startup/original/frame.ppm').exists())
        self.assertTrue((self.root / 'result/startup/patched/drive').is_dir())
        self.assertTrue(json.loads((self.root / 'result/summary.json').read_text())['passed'])

    def test_multiple_checkpoints_compare_baselines_and_patch_from_entry(self):
        case = replay_suite.validate_manifest(manifest(checkpoints=[1000, 4000],
            guest_hashes=[{'address': '0x300', 'length': 4}]), PATCHES)[0]
        case['arguments'] = ['--late-hit']
        summary = self.run_case(case, baseline_repeats=2)
        self.assertTrue(summary['passed'])
        samples = summary['checks'][0]['checkpoints']
        self.assertEqual([sample['instructions'] for sample in samples],
                         [1000, 4000, 10000])
        self.assertEqual([sample['patched']['patch_hits']['0x20'] for sample in samples],
                         [0, 0, 3])
        for instructions in [1000, 4000, 10000]:
            folder = self.root / f'result/startup/at-{instructions}'
            self.assertTrue((folder / 'original/frame.ppm').exists())
            self.assertTrue((folder / 'repeat-original/stdout.log').exists())
            self.assertTrue((folder / 'patched/stdout.log').exists())

    def test_pre_video_checkpoints_compare_guest_ram_without_framebuffer(self):
        case = replay_suite.validate_manifest(manifest(checkpoints=[1000],
            guest_hashes=[{'address': '0x300', 'length': 4}],
            allow_no_framebuffer=True), PATCHES)[0]
        case['arguments'] = ['--no-frame']
        summary = self.run_case(case, baseline_repeats=2)
        self.assertTrue(summary['passed'])
        self.assertIsNone(summary['checks'][0]['checkpoints'][0]['patched']['frame_sha256'])
        self.assertIsNotNone(summary['checks'][0]['checkpoints'][1]['patched']['frame_sha256'])

    def test_missing_frame_requires_opt_in_and_asymmetric_frame_fails(self):
        data = manifest(limit=4000, guest_hashes=[{'address': '0x300', 'length': 4}])
        case = replay_suite.validate_manifest(data, PATCHES)[0]
        case['arguments'] = ['--no-frame']
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertIn('framebuffer was not saved', summary['error'])

    def test_asymmetric_frame_presence_fails_even_when_optional(self):
        case = replay_suite.validate_manifest(manifest(
            guest_hashes=[{'address': '0x300', 'length': 4}],
            allow_no_framebuffer=True), PATCHES)[0]
        case['arguments'] = ['--patch-no-frame']
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertEqual(summary['checks'][0]['failures'], ['framebuffer SHA256 differs'])

    def test_intermediate_ram_difference_fails_even_if_final_matches(self):
        case = replay_suite.validate_manifest(manifest(checkpoints=[1000, 4000],
            guest_hashes=[{'address': '0x300', 'length': 4}]), PATCHES)[0]
        case['arguments'] = ['--early-memory-difference']
        summary = self.run_case(case, baseline_repeats=2)
        self.assertFalse(summary['passed'])
        self.assertEqual(summary['checks'][0]['failures'],
                         ['at 1000 instructions: guest RAM hash differs at 0x300:4',
                          'at 4000 instructions: guest RAM hash differs at 0x300:4'])
        self.assertTrue(summary['checks'][0]['checkpoints'][-1]['passed'])

    def test_unexecuted_patch_rejected_even_if_frames_match(self):
        case = replay_suite.validate_manifest(manifest(), PATCHES)[0]
        case['arguments'] = ['--mouse-event', '0:1:2:3', '--no-hit']
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertIn('not executed', summary['checks'][0]['failures'][0])

    def test_framebuffer_divergence_and_error_reported(self):
        case = replay_suite.validate_manifest(manifest(), PATCHES)[0]
        case['arguments'] = ['--different-frame']
        # Both runs see identical extra arguments; use fake runner's patch-only flag.
        self.runner.write_text(self.runner.read_text().replace(
            "if '--different-frame' in a", "if patched and '--different-frame' in a"))
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertIn('framebuffer SHA256 differs', summary['checks'][0]['failures'][0])

    def test_guest_ram_divergence_is_detected_independently_of_framebuffer(self):
        case = replay_suite.validate_manifest(manifest(guest_hashes=[
            {'address': '0x300', 'length': 4}]), PATCHES)[0]
        case['arguments'] = ['--different-memory']
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertEqual(summary['checks'][0]['failures'],
                         ['guest RAM hash differs at 0x300:4'])

    def test_runner_failure_creates_report_and_refuses_overwrite(self):
        case = replay_suite.validate_manifest(manifest(), PATCHES)[0]
        case['arguments'] = ['--no-status']
        self.runner.write_text(self.runner.read_text().replace(
            "print('status=instruction_limit", "if '--no-status' not in a: print('status=instruction_limit"))
        summary = self.run_case(case)
        self.assertFalse(summary['passed'])
        self.assertIn('exactly one status', summary['error'])
        with self.assertRaises(FileExistsError):
            self.run_case(case)


if __name__ == '__main__':
    unittest.main()
