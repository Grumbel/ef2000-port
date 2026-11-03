/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */
.code32
.section .text
image_start:
.org 0x86
    jmp start
.org 0x260
start:
    pushl %ds
    pushl %cs
    popl %ds
    xorl %ecx, %ecx
    movl $handler-image_start, %edx
    movl $0x2533, %eax
    int $0x21
    popl %ds
    movl $0x2532, %eax
    xorl %ecx, %ecx
    int $0x21
    movl %ebx, 0x410
    movw %es, 0x414
    call save
    movl $0x12345678, %eax
    movl $0x11223344, %ebx
    xorl %ecx, %ecx
    xorl %edx, %edx
    idivl %ecx
resume:
    movl %eax, 0x480
    movl %ebx, 0x484
    movl %ecx, 0x488
    movl %edx, 0x48c
    call save
    movl $0x4c00, %eax
    int $0x21
save:
    pushal
    xorl %ecx, %ecx
    movl $0x500, %edx
    movl $0x3c00, %eax
    int $0x21
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0xa0, %ecx
    movl $0x4000, %eax
    int $0x21
    movl $0x3e00, %eax
    int $0x21
    popal
    ret
handler:
    movl $0x454e4f44, 0x408
    movw %ds, 0x418
    movw %ss, 0x41a
    movl %esp, 0x41c
    pushal
    pushl %es
    pushl %ds
    popl %es
    leal 36(%esp), %esi
    movl $0x420, %edi
    movl $8, %ecx
    ss rep movsl
    movl 48(%esp), %esi
    movl %esi, %ebp
    movl $0x440, %edi
    movl $16, %ecx
    ss rep movsl
    movl $resume-image_start, %ss:0x10(%ebp)
    popl %es
    popal
    iret
.org 0x400
.ascii "XCEP"
.long 1
.zero 0x98
.org 0x500
.asciz "EXCEPT.BIN"
