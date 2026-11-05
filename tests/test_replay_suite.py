# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free replay suite checks using a deterministic synthetic guest runner."""

import json
from pathlib import Path
import tempfile
import unittest

from tools import replay_suite


PATCHES = {0x20}


def manifest(*, name='startup', hits=None, events=None, limit=10000):
    return {'schema': replay_suite.SCHEMA, 'cases': [
        {'name': name, 'instructions': limit, 'timer_instructions': 1000,
         'events': events or [], 'must_hit': hits if hits is not None else ['0x20']} ]}


def fake_runner(path):
    path.write_text('''#!/usr/bin/env python3
import pathlib
import sys
a = sys.argv
patched = '--patch' in a
output = pathlib.Path(a[a.index('--framebuffer') + 1])
output.write_bytes(b'P6\\n1 1\\n255\\n' + (b'\\x01\\x02\\x03' if '--different-frame' in a else b'\\x00\\x00\\x00'))
print('status=instruction_limit interrupts=19 ticks=12 irq0=10 ip=0x101 ax=0x0')
if patched:
    print('patch_hit: addr=0x20 count=' + ('0' if '--no-hit' in a else '3'))
    print('patch_hits: total=' + ('0' if '--no-hit' in a else '3'))
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

    def test_fail_closed_patch_hit_parser(self):
        status = 'status=instruction_limit interrupts=3 ticks=4 irq0=5 ip=0xff ax=0x0\n'
        with self.assertRaisesRegex(ValueError, 'count reports'):
            replay_suite.parse_output(status, PATCHES)
        with self.assertRaisesRegex(ValueError, 'unexpectedly'):
            replay_suite.parse_output(status + 'patch_hits: total=0\n', set())
        with self.assertRaisesRegex(ValueError, 'counts disagree'):
            replay_suite.parse_output(status + 'patch_hit: addr=0x20 count=1\n'
                                      'patch_hits: total=2\n', PATCHES)


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
        case = replay_suite.validate_manifest(manifest(), PATCHES)[0]
        summary = self.run_case(case, baseline_repeats=2)
        self.assertTrue(summary['passed'])
        self.assertEqual(summary['checks'][0]['patched']['patch_hits'], {'0x20': 3})
        self.assertTrue((self.root / 'result/startup/original/frame.ppm').exists())
        self.assertTrue((self.root / 'result/startup/patched/drive').is_dir())
        self.assertTrue(json.loads((self.root / 'result/summary.json').read_text())['passed'])

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
