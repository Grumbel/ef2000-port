/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

.code32
.section .text
.global _start
_start:
    cld
    call setup
    movw $7, %ax
    movw %ax, %ds
    movw %ax, %es
.ifdef CHECK_SEGMENT_LIMIT
    movl 0x40, %eax
.endif
    movw $0xf, %ax
    movw %ax, %fs
    movw $0x17, %ax
    movw %ax, %gs
    movl $0x7ff000, %esp
    movl $0x86, %eax
    jmp *%eax

.global signal_return
signal_return:
    movl $173, %eax
    int $0x80

.section .dos,"aw",@nobits
.zero 0x10000
.section .note.GNU-stack,"",@progbits
