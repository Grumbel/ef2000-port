/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

.code32
.section .text
.global _start
_start:
    cld
    movl %esp, %eax
    subl $12, %esp
    pushl %eax
    call setup
    addl $16, %esp
    ljmp $0x2f, $0x4000000

.global signal_return
signal_return:
    movl $173, %eax
    int $0x80

.include "guest_boot.inc"

.section .dos,"aw",@nobits
.zero 0x10000
.section .conventional,"aw",@nobits
.zero 0x100000
.section .note.GNU-stack,"",@progbits
