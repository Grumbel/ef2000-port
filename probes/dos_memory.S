/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

/* Allocate 256 paragraphs, then request an impossible 65535 paragraphs.
 * DOS reclaims the first block on exit. Two 32-byte register/flags records.
 */
.code32
.section .text
.org 0x86
    movl $0x410, %ebp
    movl $0x00190100, %ebx
    call allocate
    movl $0x0019ffff, %ebx
    call allocate
    movl $0x454e4f44, 0x450
    xorl %ecx, %ecx
    movl $0x500, %edx
    movw $0x3c00, %ax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0x54, %ecx
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
allocate:
    movl $0x12344800, %eax
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
    leal 32(%ebp), %ebp
    ret
.org 0x400
.ascii "DMEM"
.long 1
.zero 0x4c
.org 0x500
.asciz "DOSMEM.BIN"
