/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Query, replace, query, restore the unused INT 80h real/protected vectors.
 * Each 40-byte record: EAX/EBX/ECX/EDX/ESI/EDI, flags out/in, ES/DS (u32).
 * Only executes in a disposable copy through reference_run.py.
 */
.code32
.section .text
.org 0x86
    movl $0x410, %ebp
    movl $0x12342503, %eax
    call query
    movl %ebx, 0x408
    movl $0x12342502, %eax
    call query
    movl %ebx, 0x40c
    movw %es, 0x404
    movl $0x12342505, %eax
    movl $0x12345678, %ebx
    call change
    movl $0x12342503, %eax
    call query
    movl $0x12342505, %eax
    movl 0x408, %ebx
    call change
    movl $0x12342504, %eax
    movl $handler, %edx
    call change
    movl $0x12342502, %eax
    call query
    movl $0x12342504, %eax
    movl 0x40c, %edx
    pushl %ds
    movw 0x404, %ds
    call change
    popl %ds
    movl $0x1234252b, %eax
    movl $0x501, %ebx
    call residency
    movl $0x1234252b, %eax
    movl $0x601, %ebx
    call residency
    movl $0x454e4f44, 0x5a0
    xorl %ecx, %ecx
    movl $0x5b0, %edx
    movw $0x3c00, %ax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0x1a4, %ecx
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
query:
    movl $0x11223344, %ebx
    movl $0x99aabbcc, %edx
change:
    movl $0x55667780, %ecx
    jmp prepare
residency:
    pushl %ds
    popl %es
    movl $0x86, %ecx
    movl $0x200, %edx
prepare:
    movl $0x12345678, %esi
    movl $0x89abcdef, %edi
    cmpl %eax, %eax
    stc
    pushfl
    popl %ss:28(%ebp)
    int $0x21
    movl %eax, %ss:0(%ebp)
    movl %ebx, %ss:4(%ebp)
    movl %ecx, %ss:8(%ebp)
    movl %edx, %ss:12(%ebp)
    movl %esi, %ss:16(%ebp)
    movl %edi, %ss:20(%ebp)
    pushfl
    popl %ss:24(%ebp)
    movw %es, %ax
    movzwl %ax, %eax
    movl %eax, %ss:32(%ebp)
    movw %ds, %ax
    movzwl %ax, %eax
    movl %eax, %ss:36(%ebp)
    leal 40(%ebp), %ebp
    ret
handler:
    iret
.org 0x400
.ascii "XVEC"
.word 0, 1
.long 0, 0
.zero 0x194
.org 0x5b0
.asciz "XVECTORS.BIN"
