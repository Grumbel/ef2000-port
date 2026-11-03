# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import hashlib
import importlib.util
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

from tools.run_startup import analyze_calls


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() in ('x86_64', 'amd64') and
                     all(shutil.which(command) for command in ('gcc', 'as', 'ld')),
                     'requires x86-64 Linux and GCC/binutils')
class RuntimeServiceTests(unittest.TestCase):
    """Exercise the actual native signal/selector/syscall bridge without game bytes."""

    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='ef2000-services-')
        cls.addClassCleanup(cls.directory.cleanup)
        cls.output = Path(cls.directory.name)
        cls.runtime = Path(__file__).resolve().parent.parent / 'runtime'
        cls.assets = cls.output / 'assets'
        cls.assets.mkdir()
        (cls.output / 'drive/EF2000').mkdir(parents=True)
        cls.contents = b'EF2000 test file\n'
        (cls.assets / 'DATA.BIN').write_bytes(cls.contents)
        flags = ['gcc', '-m64', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-ffreestanding', '-fno-pie', '-fno-pic', '-fno-stack-protector',
                 '-fno-asynchronous-unwind-tables', '-fno-builtin', '-D_GNU_SOURCE', '-DEF_NATIVE64']
        for name in ('startup', 'x32'):
            cls.command(flags + ['-c', str(cls.runtime / (name + '.c')), '-o', name + '.o'])
        cls.command(['as', '--64', '-I', str(cls.runtime), str(cls.runtime / 'native64.S'), '-o', 'entry.o'])

    @classmethod
    def command(cls, command):
        subprocess.run(command, cwd=cls.output, check=True, capture_output=True, timeout=30)

    def run_guest(self, instructions, real_mode=False):
        (self.output / 'guest.S').write_text(
            '.section .guest,"awx",@progbits\n.code32\n.org 0x86\n' + instructions +
            '\nmovl $0xffff, %eax\nint $0x21\n'
            'failed:\nmovl $0xdead, %eax\nint $0x21\n'
            '.org 0x2000\n.asciz "data.bin"\n'
            '.org 0x2100\n.asciz "missing.bin"\n'
            '.org 0x2200\n.asciz "new.bin"\n'
            '.org 0x195fb0\n.section .guest_bss,"aw",@nobits\n'
            '.zero 0x4000000-0x195fb0\n.section .note.GNU-stack,"",@progbits\n')
        self.command(['as', '--64', 'guest.S', '-o', 'guest.o'])
        self.command(['ld', '-m', 'elf_x86_64', '-T', str(self.runtime / 'native64.ld'),
                      'entry.o', 'startup.o', 'x32.o', 'guest.o', '-o', 'services.elf'])
        command = [str(self.output / 'services.elf'), str(self.assets)]
        if real_mode:
            from tools.real_mode import run as run_real_mode
            with (self.output / 'calls.log').open('w') as log:
                run, _ = run_real_mode(command, self.output, log, timeout=10)
            transcript = (self.output / 'calls.log').read_text()
            error = ''
        else:
            run = subprocess.run(command, cwd=self.output, capture_output=True, text=True, timeout=10)
            transcript, error = run.stdout, run.stderr
        self.assertEqual(run.returncode, 20, transcript + error)
        result = analyze_calls(transcript)
        self.assertEqual(result['stops'][-1]['AX'], 0xffff, transcript)
        return result

    def test_registered_x32_divide_handler_changes_saved_resume_ip(self):
        self.run_guest("""
            jmp begin
        exception:
            pushl %ebp
            movl %esp, %ebp
            pushl %eax
            movl 16(%ebp), %ebp
            cmpw $2, 14(%ebp)
            jne failed
            cmpl $0x2f, 20(%ebp)
            jne failed
            movl $resume-0x10000000, 16(%ebp)
            popl %eax
            popl %ebp
            iret
        begin:
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2533, %eax
            xorl %ecx, %ecx
            movl $exception-0x10000000, %edx
            int $0x21
            popl %ds
            movl $0x12345678, %eax
            movl $0x89abcdef, %ebx
            xorl %ecx, %ecx
            xorl %edx, %edx
            idivl %ecx
            jmp failed
        resume:
            cmpl $0x12345678, %eax
            jne failed
            cmpl $0x89abcdef, %ebx
            jne failed
            testl %ecx, %ecx
            jne failed
        """)

    def test_signed_renderer_overflow_uses_original_style_recovery(self):
        self.run_guest("""
            jmp begin
        exception:
            pushl %ebp
            movl %esp, %ebp
            pushl %eax
            movl 16(%ebp), %ebp
            movl $resume-0x10000000, 16(%ebp)
            popl %eax
            popl %ebp
            iret
        begin:
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2533, %eax
            xorl %ecx, %ecx
            movl $exception-0x10000000, %edx
            int $0x21
            popl %ds
            movl $0x000d2e06, %eax
            movl $0x0000000d, %edx
            movl $22, %esi
            idivw %si
            jmp failed
        resume:
            cmpl $0x000d2e06, %eax
            jne failed
            cmpl $13, %edx
            jne failed
            cmpl $22, %esi
            jne failed
        """)

    def test_clock_polling_cannot_advance_minimum_physics_ticks_too_fast(self):
        report = self.run_guest("""
            movl $0x2c00, %eax
            int $0x21
            movl $0x2c00, %eax
            int $0x21
            movl $0x2c00, %eax
            int $0x21
        """)
        samples = [s['SECONDS'] * 1000000000 + s['NANOSECONDS']
                   for s in report['clock_samples']]
        self.assertEqual(len(samples), 3)
        for start, end in zip(samples, samples[1:]):
            self.assertGreaterEqual(end - start, 18000000)

    def test_dos_date_and_time_from_arbitrary_call_sites(self):
        from datetime import datetime, timezone
        before = datetime.now(timezone.utc)
        result = self.run_guest("""
            movl $0xabcd2ab8, %eax
            movl $0x12340000, %ecx
            movl $0x56780000, %edx
            int $0x21
            movl $0x2c00, %eax
            int $0x21
        """)
        after = datetime.now(timezone.utc)
        date, clock = result['returns'][-2:]
        actual = (date['CX'] & 65535, (date['DX'] >> 8) & 255, date['DX'] & 255)
        self.assertIn(actual, [tuple(t.timetuple()[:3]) for t in (before, after)])
        self.assertEqual(date['AX'] & 0xffffff00, 0xabcd2a00)
        self.assertEqual(date['CX'] >> 16, 0x1234)
        self.assertEqual(date['DX'] >> 16, 0x5678)
        self.assertLess(date['AX'] & 255, 7)
        self.assertLess((clock['CX'] >> 8) & 255, 24)
        self.assertLess(clock['CX'] & 255, 60)
        self.assertLess((clock['DX'] >> 8) & 255, 60)
        self.assertLess(clock['DX'] & 255, 100)

    def test_commit_file_and_invalid_handle(self):
        self.run_guest("""
            movl $0x2200, %edx
            movl $0x3c00, %eax
            int $0x21
            jc failed
            movl %eax, %ebx
            movl $0x6800, %eax
            int $0x21
            jc failed
            movl $0x3e00, %eax
            int $0x21
            movl $0x6800, %eax
            int $0x21
            jnc failed
            cmpl $6, %eax
            jne failed
        """)

    def test_read_seek_eof_close_missing_and_source_unchanged(self):
        before = hashlib.sha256((self.assets / 'DATA.BIN').read_bytes()).hexdigest()
        report = self.run_guest('''
            movl $0x2000, %edx
            movl $0x3d00, %eax
            int $0x21
            jc failed
            movl %eax, %ebx
            movl $0x99aabbcc, %edx
            movl $0x4400, %eax
            int $0x21
            movl $0, %ecx
            movl $0, %edx
            movl $0x4202, %eax
            int $0x21
            movl $0x4200, %eax
            movl $0, %edx
            int $0x21
            movl $0x3000, %edx
            movl $1024, %ecx
            movl $0x3f00, %eax
            int $0x21
            cmpl $0x30324645, 0x3000
            jne failed
            movl $0x3000, %edx
            movl $1024, %ecx
            movl $0x3f00, %eax
            int $0x21
            movl $0x3e00, %eax
            int $0x21
            movl $0x3e00, %eax
            int $0x21
            movl $0x2100, %edx
            movl $0x3d00, %eax
            int $0x21
            movl $0x2000, %edx
            movl $0x3d00, %eax
            int $0x21
            movl %eax, %ebx
            movl $0x3e00, %eax
            int $0x21
        ''')
        returns = report['returns']
        self.assertEqual([r['AX'] for r in returns],
                         [5, 2, len(self.contents), 0, len(self.contents), 0, 0x3e01, 6, 2, 5, 0x3e01])
        self.assertEqual(returns[1]['DX'], 0x99aa0002)
        self.assertEqual((returns[4]['CX'], returns[4]['DX']), (0, 0))
        self.assertTrue(returns[7]['FLAGS'] & 1)
        self.assertTrue(returns[8]['FLAGS'] & 1)
        self.assertEqual(hashlib.sha256((self.assets / 'DATA.BIN').read_bytes()).hexdigest(), before)

    def test_conventional_memory_is_addressable_and_exhaustion_is_reported(self):
        report = self.run_guest('''
            movl $0x190100, %ebx
            movl $0x4800, %eax
            int $0x21
            jc failed
            movl $0x12345678, (%ebx)
            cmpl $0x12345678, 0xf1010000
            jne failed
            movl $0x19ffff, %ebx
            movl $0x4800, %eax
            int $0x21
        ''')
        success, failure = report['returns']
        self.assertEqual((success['AX'], success['BX']), (0x1000, 0xf1010000))
        self.assertEqual((failure['AX'], failure['BX']), (8, 0x7f00))
        self.assertTrue(failure['FLAGS'] & 1)

    def test_descriptor_mapping_limit_and_reuse(self):
        report = self.run_guest('''
            movl $0x3501, %eax
            movl $0x80, %ecx
            int $0x21
            jc failed
            movl $0x3503, %eax
            movl $0x1010000, %ecx
            int $0x21
            movl $0x3505, %eax
            movl $0x80, %ecx
            int $0x21
            movl $0x3504, %eax
            int $0x21
            movw %bx, %fs
            movb $0x5a, %fs:0x20
            cmpb $0x5a, 0xf1010020
            jne failed
            lsll %ebx, %eax
            cmpl $0x80, %eax
            jne failed
            movw $0, %ax
            movw %ax, %fs
            movl $0x3502, %eax
            int $0x21
            movl $0x3501, %eax
            int $0x21
        ''')
        returns = report['returns']
        self.assertEqual(returns[0]['BX'], 0x37)
        self.assertEqual(returns[2]['CX'], 0x80)
        self.assertEqual(returns[3]['CX'], 0x1010000)
        self.assertEqual(returns[5]['BX'], 0x37)

    def test_vector_and_exception_registration_round_trip(self):
        report = self.run_guest('''
            movl $0x12345678, %ebx
            movl $0x55667780, %ecx
            movl $0x2505, %eax
            int $0x21
            movl $0x2503, %eax
            int $0x21
            movl $0x200, %edx
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2504, %eax
            int $0x21
            popl %ds
            movl $0x2502, %eax
            int $0x21
            movl $0, %ecx
            movl $0x210, %edx
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2533, %eax
            int $0x21
            popl %ds
            movl $0x2532, %eax
            int $0x21
            movl $0x2502, %eax
            int $0x21
        ''')
        returns = report['returns']
        self.assertEqual(returns[1]['BX'], 0x12345678)
        self.assertEqual((returns[3]['BX'], returns[3]['ES']), (0x200, 0x2f))
        self.assertEqual((returns[5]['BX'], returns[5]['ES']), (0x210, 0x2f))
        self.assertEqual((returns[6]['BX'], returns[6]['ES']), (0x210, 0x2f))

    def test_keyboard_irq_delivers_make_break_and_extended_bytes(self):
        self.run_guest("""
            jmp begin
        irq:
            pushal
            movl $0x60, %edx
            xorl %eax, %eax
            inb $0x60, %al
            movl 0x5000, %ecx
            movb %al, 0x5004(%ecx)
            incl 0x5000
            movl $0x61, %edx
            inb %dx, %al
            movb %al, %ah
            orb $0x80, %al
            outb %al, %dx
            movb %ah, %al
            outb %al, %dx
            movb $0x20, %al
            outb %al, $0x20
            popal
            iret
        begin:
            cli
            movl $0, 0x5000
            movl $9, %ecx
            movl $irq-0x10000000, %edx
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2504, %eax
            int $0x21
            popl %ds
            pushl %gs
            movl $0x1f, %eax
            movw %ax, %gs
            movl $0x31504e49, %gs:0xf1400
            movl $0x00060000, %gs:0xf1404
            movl $0x48e09e1e, %gs:0xf1408
            movw $0xc8e0, %gs:0xf140c
            popl %gs
            movb $0x36, %al
            outb %al, $0x43
            movb $0, %al
            outb %al, $0x40
            movb $0x10, %al
            outb %al, $0x40
            sti
        wait:
            cmpl $6, 0x5000
            jb wait
            cmpl $0x48e09e1e, 0x5004
            jne failed
            cmpw $0xc8e0, 0x5008
            jne failed
        """)

    def test_gameport_charge_axis_discharge_buttons_and_absence(self):
        self.run_guest("""
            movl $0x201, %edx
            inb %dx, %al
            cmpb $0xff, %al
            jne failed
            pushl %gs
            movl $0x1f, %eax
            movw %ax, %gs
            movl $0x31504e49, %gs:0xf1400
            movw $0x0503, %gs:0xf1600
            movw $1, %gs:0xf1602
            movw $3, %gs:0xf1604
            outb %al, %dx
            inb %dx, %al
            cmpb $0xaf, %al
            jne failed
            inb %dx, %al
            cmpb $0xae, %al
            jne failed
            inb %dx, %al
            cmpb $0xae, %al
            jne failed
            inb %dx, %al
            cmpb $0xac, %al
            jne failed
            movb $0, %gs:0xf1600
            inb %dx, %al
            cmpb $0xff, %al
            jne failed
            popl %gs
        """)

    def test_keyboard_acknowledgement_and_pit_programming(self):
        report = self.run_guest('''
            cli
            movl $0x64, %edx
            inb %dx, %al
            cmpb $0, %al
            jne failed
            movl $0x60, %edx
            movb $0xed, %al
            outb %al, %dx
            movl $0x64, %edx
            inb %dx, %al
            cmpb $1, %al
            jne failed
            movl $0x60, %edx
            inb %dx, %al
            cmpb $0xfa, %al
            jne failed
            movl $0x64, %edx
            inb %dx, %al
            cmpb $0, %al
            jne failed
            movb $0x36, %al
            outb %al, $0x43
            movb $0x40, %al
            outb %al, $0x40
            movb $0, %al
            outb %al, $0x40
            sti
        ''')
        self.assertEqual(report['pit'][-1]['DIVISOR'], 64)
        self.assertEqual([item['VALUE'] for item in report['port_io']], [0, 0xed, 1, 0xfa, 0])

    def test_write_existing_file_uses_overlay_and_create_is_local(self):
        source = (self.assets / 'DATA.BIN').read_bytes()
        self.run_guest('''
            movl $0x2000, %edx
            movl $0x3d02, %eax
            int $0x21
            jc failed
            movl %eax, %ebx
            movl $0x44434241, 0x3000
            movl $0x3000, %edx
            movl $4, %ecx
            movl $0x4000, %eax
            int $0x21
            jc failed
            movl $0x3e00, %eax
            int $0x21
            movl $0x2200, %edx
            movl $0, %ecx
            movl $0x3c00, %eax
            int $0x21
            jc failed
            movl %eax, %ebx
            movl $0x3000, %edx
            movl $4, %ecx
            movl $0x4000, %eax
            int $0x21
            jc failed
            movl $0x3e00, %eax
            int $0x21
        ''')
        self.assertEqual((self.assets / 'DATA.BIN').read_bytes(), source)
        self.assertEqual((self.output / 'drive/EF2000/DATA.BIN').read_bytes(), b'ABCD' + source[4:])
        self.assertEqual((self.output / 'drive/EF2000/NEW.BIN').read_bytes(), b'ABCD')
        # Each test starts with the immutable fixture rather than this overlay.
        (self.output / 'drive/EF2000/DATA.BIN').unlink()
        (self.output / 'drive/EF2000/NEW.BIN').unlink()

    def test_native_cpu_alignment_fault_resumes_the_original_store(self):
        self.run_guest('''
            pushfl
            orl $0x40000, (%esp)
            popfl
            movl $0x12345678, 0x5001
            cmpl $0x12345678, 0x5001
            jne failed
        ''')

    def test_timer_delivers_repeated_guest_interrupts_and_preserves_registers(self):
        self.run_guest('''
            jmp begin
        irq:
            incl 0x5000
            pushl %eax
            movb $0x20, %al
            outb %al, $0x20
            sti
            popl %eax
            iret
        begin:
            movl $0, 0x5000
            movl $8, %ecx
            movl $irq-0x10000000, %edx
            pushl %ds
            pushl %cs
            popl %ds
            movl $0x2504, %eax
            int $0x21
            popl %ds
            cli
            movb $0x36, %al
            outb %al, $0x43
            movb $0, %al
            outb %al, $0x40
            movb $0x10, %al
            outb %al, $0x40
            movl $0x12345678, %eax
            movl $0x89abcdef, %ebx
            sti
        wait:
            cmpl $3, 0x5000
            jb wait
            cmpl $0x12345678, %eax
            jne failed
            cmpl $0x89abcdef, %ebx
            jne failed
        ''', real_mode=bool(importlib.util.find_spec('unicorn')))

    @unittest.skipUnless(importlib.util.find_spec('unicorn'), 'requires Unicorn real-mode bridge')
    def test_native_bios_bridge_updates_original_registers(self):
        report = self.run_guest('''
            movl $0x0f00, %eax
            int $0x10
            cmpl $0x5003, %eax
            jne failed
            movl $0x13, %eax
            int $0x10
            movl $0x0f00, %eax
            int $0x10
            cmpl $0x2813, %eax
            jne failed
        ''', real_mode=True)
        self.assertEqual([item['AX'] for item in report['returns']], [0x5003, 0x13, 0x2813])
