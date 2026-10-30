# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import hashlib
import importlib.util
import json
import os
import platform
from pathlib import Path
import shutil
import subprocess
import struct
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
        cls.asset_hashes = {name: hashlib.sha256((cls.executable.parent / name).read_bytes()).hexdigest()
                            for name in ('DID1ST.DAT', 'DID.DAT', 'EF2000.CFG')}
        output = Path(cls.directory.name) / 'run'
        runner = Path(__file__).resolve().parent.parent / 'tools' / 'run_startup.py'
        cls.diagnostic_args = ['--real-mode', 'stop', '--sound', 'probe', '--headless', '--seconds', '15']
        run = subprocess.run([sys.executable, str(runner), str(cls.executable), '--output', str(output)] + cls.diagnostic_args,
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
        self.assertEqual(self.report['stops'][-1]['IP'], 0x1112b7)
        self.assertEqual(self.report['stops'][-1]['AX'], 0x2511)
        self.assertEqual(self.report['returns'][3]['CX'], 0x4e0000)

    def test_original_renderer_overflow_executes_its_unmodified_recovery_handler(self):
        # Instrument only a disposable extracted image's entry. The exact fault
        # instruction, exception handler, and recovery code remain original.
        output = Path(self.directory.name) / 'renderer-overflow'
        output.mkdir()
        built = Path(self.directory.name) / 'run'
        runtime = self.runner.parent.parent / 'runtime'
        (output / 'hook.S').write_text("""
            .code32
            .section .text
        image_start:
            .org 0x86
            jmp begin
            .org 0x260
        begin:
            movw %cs, 0x17bf74
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2533, %eax
            xorl %ecx, %ecx
            movl $0xfcc6f, %edx
            int $0x21
            popl %ds
            movl $0x000d2e06, %eax
            movl $13, %edx
            movl $22, %esi
            pushl $recovered-image_start
            movl $0xc5a7c, %edi
            jmp *%edi
        recovered:
            cmpl $0x000d2e06, %eax
            jne failed
            cmpl $13, %edx
            jne failed
            movl $0xffff, %eax
            int $0x21
        failed:
            movl $0xdead, %eax
            int $0x21
        """)
        for command in (['as', '--32', 'hook.S', '-o', 'hook.o'],
                        ['objcopy', '-O', 'binary', '--only-section=.text', 'hook.o', 'hook.bin']):
            subprocess.run(command, cwd=output, capture_output=True, check=True, timeout=10)
        payload = bytearray((built / 'payload.bin').read_bytes())
        hook = (output / 'hook.bin').read_bytes()
        self.assertLess(len(hook), 0x400)
        payload[:len(hook)] = hook
        (output / 'payload.bin').write_bytes(payload)
        shutil.copyfile(built / 'payload.S', output / 'payload.S')
        subprocess.run(['as', '--64', 'payload.S', '-o', 'payload.o'],
                       cwd=output, capture_output=True, check=True, timeout=10)
        command = ['ld', '-m', 'elf_x86_64', '-T', str(runtime / 'native64.ld'),
                   *(str(built / name) for name in ('entry.o', 'startup.o', 'x32.o')),
                   'payload.o', '-o', 'overflow.elf']
        subprocess.run(command, cwd=output, capture_output=True, check=True, timeout=10)
        run = subprocess.run([str(output / 'overflow.elf'), str(self.executable.parent)],
                             cwd=output, capture_output=True, text=True, timeout=10)
        self.assertEqual(run.returncode, 20, run.stdout + run.stderr)
        self.assertIn('EXCEPTION ENTER VECTOR=00000000 IP=000c5a7c HANDLER=000fcc6f', run.stdout)
        self.assertIn('EXCEPTION RETURN VECTOR=00000000 IP=000c5acd', run.stdout)
        self.assertEqual(analyze_calls(run.stdout)['stops'][-1]['AX'], 0xffff)

    def test_original_assets_and_input_initialization_are_reached(self):
        self.assertIn('DID1ST.DAT', self.report['file_requests'])
        self.assertIn('./DID.DAT', self.report['file_requests'])
        self.assertIn('EF2000.CFG', self.report['file_requests'])
        self.assertIn('TACTCOM', self.report['console'])
        self.assertTrue(any('bytes of memory' in line for line in self.report['console']))
        self.assertTrue(any(item['PORT'] == 0x60 and item['VALUE'] == 0xfa
                            for item in self.report['port_io']))
        self.assertTrue(any(item['DIVISOR'] > 0 for item in self.report['pit']))
        request, = self.report['real_mode_requests']
        self.assertEqual((request['VECTOR'], request['AX']), (0x66, 0x300))
        self.assertEqual((Path(self.directory.name) / 'run/conventional.bin').stat().st_size, 0x100000)

    def test_dos_time_matches_recorded_host_sample_and_preserves_registers(self):
        before = self.report['calls'][4]
        after = self.report['returns'][4]
        sample, = self.report['clock_samples']
        seconds = sample['SECONDS'] % 86400
        self.assertEqual(after['AX'], 0x2c00)
        self.assertEqual(after['CX'], (before['CX'] & 0xffff0000) |
                         ((seconds // 3600) << 8) | ((seconds // 60) % 60))
        self.assertEqual(after['DX'], (before['DX'] & 0xffff0000) |
                         ((seconds % 60) << 8) | (sample['NANOSECONDS'] // 10000000))
        for name in ('BX', 'SI', 'DI', 'SP', 'FLAGS', 'DS', 'SS'):
            self.assertEqual(after[name], before[name], name)

    def test_source_executable_remains_unchanged(self):
        self.assertEqual(hashlib.sha256(self.executable.read_bytes()).hexdigest(), self.before)
        for name, expected in self.asset_hashes.items():
            self.assertEqual(hashlib.sha256((self.executable.parent / name).read_bytes()).hexdigest(),
                             expected, name)

    @unittest.skipUnless(importlib.util.find_spec('unicorn'), 'requires Unicorn for BIOS/video bridge')
    def test_original_game_reaches_banked_gui_and_exports_framebuffer(self):
        output = Path(self.directory.name) / 'gui'
        run = subprocess.run([sys.executable, str(self.runner), str(self.executable),
                              '--output', str(output), '--headless', '--seconds', '20'],
                             capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)  # Deliberate time limit.
        report = json.loads((output / 'report.json').read_text())
        self.assertEqual(report['status'], 'time_limit')
        self.assertTrue(report['startup_checkpoint_verified'])
        self.assertFalse(report['faults'])
        self.assertFalse(any('error' in request for request in report['real_mode_execution']))
        video = json.loads((output / 'video.json').read_text())
        self.assertEqual((video['mode'], video['dimensions']), (0x100, [640, 400]))
        png = (output / 'framebuffer.png').read_bytes()
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack_from('>2I', png, 16), (640, 400))
        self.assertGreater(len(png), 10000)
        self.assertIn('./DID.DAT', report['file_requests'])

    def test_native_cpu_enforces_expand_down_lower_bound(self):
        output = Path(self.directory.name) / 'limits'
        run = subprocess.run([sys.executable, str(self.runner), str(self.executable),
                              '--output', str(output), '--check-segment-limit'] + self.diagnostic_args,
                             capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        report = json.loads((output / 'report.json').read_text())
        self.assertTrue(report['segment_limit_enforced'])
        self.assertEqual(report['faults'][0]['TRAP'], 13)

    @unittest.skipUnless(shutil.which('qemu-i386'), 'requires qemu-i386')
    def test_qemu_comparison_reaches_same_service_boundary(self):
        output = Path(self.directory.name) / 'qemu'
        run = subprocess.run([sys.executable, str(self.runner), str(self.executable),
                              '--output', str(output), '--backend', 'qemu'] + self.diagnostic_args,
                             capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        report = json.loads((output / 'report.json').read_text())
        self.assertTrue(report['startup_checkpoint_verified'])
        self.assertEqual(report['stops'][-1]['IP'], self.report['stops'][-1]['IP'])
        self.assertEqual(report['stops'][-1]['AX'], self.report['stops'][-1]['AX'])


if __name__ == '__main__':
    unittest.main()
