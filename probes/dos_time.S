/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Disposable-copy entry hook. Four AH=2Ch samples with sentinel registers and
 * input/output flags, separated by busy waits. No original installation edits.
 */
.code32
.section .text
.org 0x86
    movl $0x410, %ebp
sample:
    movl $0x12342cde, %eax
    movl $0x11223344, %ebx
    movl $0x55667788, %ecx
    movl $0x99aabbcc, %edx
    movl $0x12345678, %esi
    movl $0x89abcdef, %edi
    cmpl %eax, %eax
    stc
    pushfl
    popl 28(%ebp)
    int $0x21
    movl %eax, 0(%ebp)
    movl %ebx, 4(%ebp)
    movl %ecx, 8(%ebp)
    movl %edx, 12(%ebp)
    movl %esi, 16(%ebp)
    movl %edi, 20(%ebp)
    pushfl
    popl 24(%ebp)
    addl $32, %ebp
    movl $10000000, %ecx
delay:
    loop delay
    cmpl $0x490, %ebp
    jne sample
    movl $0x454e4f44, 0x490
    xorl %ecx, %ecx
    movl $0x500, %edx
    movw $0x3c00, %ax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0x94, %ecx
    movb $0x40, %ah
    int $0x21
    jc failed
    movb $0x3e, %ah
    int $0x21
    movw $0x4c00, %ax
    int $0x21
failed:
    movw $0x4c63, %ax
    int $0x21
.org 0x400
.ascii "TIME"
.long 1
.zero 0x8c
.org 0x500
.asciz "DOSTIME.BIN"
