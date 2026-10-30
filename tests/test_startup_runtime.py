# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import hashlib
import json
import os
import platform
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.run_startup import analyze_calls


class TranscriptTests(unittest.TestCase):
    def test_unsupported_exit_without_guest_checkpoint_is_not_success(self):
        self.assertFalse(analyze_calls('STOP IP=0000019a AX=00003509')['startup_checkpoint_verified'])

    def test_failed_bss_check_is_not_success(self):
        result = analyze_calls('CHECK DATA_LIMIT=00000260 BSS_ZERO=00000000 COMMAND=00000001 PROGRAM=00000001')
        self.assertEqual(result['checkpoint']['BSS_ZERO'], 0)
        self.assertFalse(result['startup_checkpoint_verified'])


@unittest.skipUnless(os.environ.get('EF2000_EXE') and
                     platform.machine() in ('x86_64', 'amd64') and
                     all(shutil.which(command) for command in ('gcc', 'as', 'ld')),
                     'requires EF2000_EXE, GCC/binutils, and x86-64 Linux')
class OriginalStartupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='ef2000-startup-')
        cls.addClassCleanup(cls.directory.cleanup)
        cls.executable = Path(os.environ['EF2000_EXE']).resolve()
        cls.before = hashlib.sha256(cls.executable.read_bytes()).hexdigest()
        output = Path(cls.directory.name) / 'run'
        runner = Path(__file__).resolve().parent.parent / 'tools' / 'run_startup.py'
        run = subprocess.run([sys.executable, str(runner), str(cls.executable), '--output', str(output)],
                             capture_output=True, text=True, timeout=45)
        if run.returncode:
            raise AssertionError(run.stdout + run.stderr)
        cls.report = json.loads((output / 'report.json').read_text())
        cls.runner = runner

    def test_original_startup_reaches_verified_allocation_boundary(self):
        self.assertEqual(self.report['returncode'], 20)
        self.assertEqual(self.report['status'], 'unsupported_interface')
        self.assertTrue(self.report['startup_checkpoint_verified'])
        self.assertEqual(self.report['checkpoint'],
                         {'DATA_LIMIT': 0x260, 'BSS_ZERO': 1, 'COMMAND': 1,
                          'PROGRAM': 1, 'PSP': 0xf, 'ENV': 0x17})
        self.assertEqual(self.report['stops'][-1]['IP'], 0x11f2a4)
        self.assertEqual((self.report['stops'][-1]['AX'] >> 8) & 255, 0x2c)
        self.assertEqual(self.report['returns'][3]['CX'], 0x4e0000)

    def test_source_executable_remains_unchanged(self):
        self.assertEqual(hashlib.sha256(self.executable.read_bytes()).hexdigest(), self.before)

    def test_native_cpu_enforces_expand_down_lower_bound(self):
        output = Path(self.directory.name) / 'limits'
        run = subprocess.run([sys.executable, str(self.runner), str(self.executable),
                              '--output', str(output), '--check-segment-limit'],
                             capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        report = json.loads((output / 'report.json').read_text())
        self.assertTrue(report['segment_limit_enforced'])
        self.assertEqual(report['faults'][0]['TRAP'], 13)

    @unittest.skipUnless(shutil.which('qemu-i386'), 'requires qemu-i386')
    def test_qemu_comparison_reaches_same_service_boundary(self):
        output = Path(self.directory.name) / 'qemu'
        run = subprocess.run([sys.executable, str(self.runner), str(self.executable),
                              '--output', str(output), '--backend', 'qemu'],
                             capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        report = json.loads((output / 'report.json').read_text())
        self.assertTrue(report['startup_checkpoint_verified'])
        self.assertEqual(report['stops'][-1]['IP'], self.report['stops'][-1]['IP'])
        self.assertEqual(report['stops'][-1]['AX'], self.report['stops'][-1]['AX'])


if __name__ == '__main__':
    unittest.main()
