/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

.code64
.section .text
.global _start
_start:
    cld
    call setup
    pushq $0x2f
    pushq $0x800000
    lretq

.global signal_return
signal_return:
    movl $15, %eax
    syscall

.section .guest_boot,"ax",@progbits
.code32
    movw $7, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x7ff000, %esp
.ifdef CHECK_SEGMENT_LIMIT
    movl 0x40, %eax /* Must fault: below DS expand-down lower bound 0x7f. */
.endif
    movw $0xf, %ax
    movw %ax, %fs
    movw $0x17, %ax
    movw %ax, %gs
    xorl %eax, %eax
    xorl %ebx, %ebx
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %esi, %esi
    xorl %edi, %edi
    xorl %ebp, %ebp
    pushl $0x202
    popfl
    movl $0x86, %eax
    jmp *%eax

.section .dos,"aw",@nobits
.zero 0x10000
.section .note.GNU-stack,"",@progbits
