/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

.section .guest_boot,"ax",@progbits
.code32
    movw $7, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x3fff000, %esp
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

.org 0x100
    /* Synthetic BIOS timer fallback: acknowledge IRQ0 and return. The host
     * timer bridge maintains the BIOS tick count in the DOS memory window.
     */
    pushl %eax
    movb $0x20, %al
    outb %al, $0x20
    sti
    popl %eax
    iret

.org 0x120
    /* Host exception-return gate. The original handler returns here by IRET. */
    int $0x21
