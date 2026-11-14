# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Exercise native port I/O in a reserved arena using synthetic guest code."""

from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(platform.system() == 'Linux' and
                     platform.machine() in ('x86_64', 'amd64') and
                     all(shutil.which(tool) for tool in ('gcc', 'as', 'ld')),
                     'requires native x86-64 Linux and GCC/binutils')
class NativeHybridIOTests(unittest.TestCase):
    def test_reserved_arena_uses_the_existing_port_models(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            runtime = ROOT / 'runtime'
            compiler = ['gcc', '-m64', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-ffreestanding', '-fno-pie', '-fno-pic', '-fno-stack-protector',
                        '-fno-asynchronous-unwind-tables', '-fno-builtin',
                        '-D_GNU_SOURCE', '-DEF_NATIVE64',
                        '-DEF_RESERVED_ARENA_START=0x3000000',
                        '-DEF_RESERVED_ARENA_END=0x3000040']
            commands = [compiler + ['-c', str(runtime / name), '-o', target]
                        for name, target in (('startup.c', 'startup.o'), ('x32.c', 'x32.o'))]
            commands.append(['as', '--64', '-I', str(runtime), str(runtime / 'native64.S'),
                             '-o', 'entry.o'])
            for command in commands:
                subprocess.run(command, cwd=output, capture_output=True, check=True, timeout=30)
            for destination, expected_exit in ((0x3000000, 26), (0x3000100, 21)):
                with self.subTest(destination=hex(destination)):
                    # Copy a small helper out of the original code region, as a
                    # hybrid patch would, then jump into it. The second location
                    # is mapped RAM outside the reserved arena and must fault.
                    (output / 'payload.S').write_text(f'''
.code32
.section .guest,"awx",@progbits
.org 0x86
    movl $0x100, %esi
    movl ${destination}, %edi
    movl $helper_end-helper, %ecx
    rep movsb
    movl ${destination}, %eax
    jmp *%eax
.org 0x100
helper:
    cli
    movl $0x61, %edx
    movb $0x5a, %al
    outb %al, %dx
    inb %dx, %al
    cmpb $0x5a, %al
    jne failed
    movl $0x64, %edx
    inb %dx, %al
    movl $0x201, %edx
    inb %dx, %al
    cmpb $0xff, %al
    jne failed
    sti
    movl $0x4c00, %eax
    jmp leave_helper
failed:
    movl $0x4c01, %eax
leave_helper:
    movl $0x180, %edx
    jmp *%edx
helper_end:
.org 0x180
    int $0x21
.section .guest_bss,"aw",@nobits
.zero 0x4000000-0x182
.section .note.GNU-stack,"",@progbits
''')
                    subprocess.run(['as', '--64', 'payload.S', '-o', 'payload.o'],
                                   cwd=output, capture_output=True, check=True, timeout=10)
                    subprocess.run(['ld', '-m', 'elf_x86_64', '-T', str(runtime / 'native64.ld'),
                                    'entry.o', 'startup.o', 'x32.o', 'payload.o', '-o', 'guest'],
                                   cwd=output, capture_output=True, check=True, timeout=10)
                    run = subprocess.run([str(output / 'guest'), str(output)], cwd=output,
                                         capture_output=True, text=True, timeout=10)
                    self.assertEqual(run.returncode, expected_exit, run.stdout + run.stderr)
                    if expected_exit == 26:
                        self.assertIn('PORT=00000064', run.stdout)
                        self.assertIn('EXIT CODE=00000000', run.stdout)
                    else:
                        self.assertIn('FAULT ', run.stdout)


if __name__ == '__main__':
    unittest.main()
