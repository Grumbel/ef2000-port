/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

.code64
.section .text
.global _start
_start:
    cld
    movq %rsp, %rdi
    call setup
    pushq $0x2f
    pushq $0x4000000
    lretq

.global signal_return
signal_return:
    movl $15, %eax
    syscall

.global signal_entry
signal_entry:
    /* CPU detection can leave AC set. Clear it before entering host C so the
     * handler itself can safely access ordinary unaligned ABI fields.
     */
    pushfq
    andq $-262145, (%rsp)
    popfq
    cld
    jmp fault_handler

.include "guest_boot.inc"

.section .dos,"aw",@nobits
.zero 0x10000
.section .conventional,"aw",@nobits
.zero 0x100000
.section .note.GNU-stack,"",@progbits
