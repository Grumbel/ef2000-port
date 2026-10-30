/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Disposable-copy probe: reset/version and read-only mixer resource registers.
 * No DMA, IRQ installation, or audio playback. SB16.BIN contains eight u32
 * samples at +10h and a DONE/FAIL marker at +8.
 */
.code32
.section .text
.org 0x86
    movl $0x226, %edx
    movb $1, %al
    outb %al, %dx
    movl $20, %ecx
delay:
    inb %dx, %al
    loop delay
    movb $0, %al
    outb %al, %dx
    call read_dsp
    movl %eax, 0x410
    movl $0x22c, %edx
    xorl %eax, %eax
    inb %dx, %al
    movl %eax, 0x414
    movb $0xe1, %al
    outb %al, %dx
    call read_dsp
    movl %eax, 0x418
    call read_dsp
    movl %eax, 0x41c
    movl $0x224, %edx
    movb $0x80, %al
    outb %al, %dx
    incl %edx
    xorl %eax, %eax
    inb %dx, %al
    movl %eax, 0x420
    decl %edx
    movb $0x81, %al
    outb %al, %dx
    incl %edx
    xorl %eax, %eax
    inb %dx, %al
    movl %eax, 0x424
    decl %edx
    movb $0x82, %al
    outb %al, %dx
    incl %edx
    xorl %eax, %eax
    inb %dx, %al
    movl %eax, 0x428
    movl $0x22e, %edx
    xorl %eax, %eax
    inb %dx, %al
    movl %eax, 0x42c
    movl $0x454e4f44, 0x408
    jmp save
read_dsp:
    movl $65536, %ecx
    movl $0x22e, %edx
ready:
    inb %dx, %al
    testb $0x80, %al
    jnz read
    loop ready
    movl $0x4c494146, 0x408
    addl $4, %esp
    jmp save
read:
    movl $0x22a, %edx
    xorl %eax, %eax
    inb %dx, %al
    ret
save:
    xorl %ecx, %ecx
    movl $0x500, %edx
    movl $0x3c00, %eax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0x30, %ecx
    movl $0x4000, %eax
    int $0x21
    movl $0x4c00, %eax
    int $0x21
failed:
    movl $0x4c63, %eax
    int $0x21
.org 0x400
.ascii "SBDP"
.long 1
.zero 0x28
.org 0x500
.asciz "SB16.BIN"
