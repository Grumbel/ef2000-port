/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Instrumentation for a disposable executable copy; runs the ORIGINAL extender.
 * All addresses are application offsets. This replaces startup and exits before
 * runtime initializers or game code. Output layout: tools/read_x32_probe.py.
 */
.code32
.section .text
.org 0x86
    movl %esp, 0x408
    movw %cs, 0x40c
    movw %ds, 0x40e
    movw %es, 0x410
    movw %fs, 0x412
    movw %gs, 0x414
    movw %ss, 0x416
    pushfl
    popl 0x418
    movw %ds, %ax
    lsll %eax, %eax
    movl %eax, 0x41c
    movw %ss, %ax
    lsll %eax, %eax
    movl %eax, 0x420
    movw %fs, %ax
    lsll %eax, %eax
    movl %eax, 0x424
    movw %gs, %ax
    lsll %eax, %eax
    movl %eax, 0x428
    movw %ds, %ax
    larl %eax, %eax
    movl %eax, 0x42c
    movzwl 0x120cc4, %eax
    movl %eax, 0x430
    movl 0x120cc6, %eax
    movl %eax, 0x434
    movzwl 0x132a24, %eax
    movl %eax, 0x438

    movw %ds, %bx
    movl $0x260, %ecx
    movw $0x3505, %ax
    int $0x21
    pushfl
    popl 0x43c
    movl %eax, 0x440
    movl %ecx, 0x444
    movw %ds, %ax
    lsll %eax, %eax
    movl %eax, 0x448
    movw %ds, %ax
    larl %eax, %eax
    movl %eax, 0x44c
    pushl %ds
    popl %ss

    leal -0x200(%esp), %ebx
    movl $0x4000, %ecx
    movl $0x4df000, %esi
    movl %ebx, 0x450
    movl %ecx, 0x454
    movl %esi, 0x458
    pushl %es
    movw 0x132a24, %ax
    movw %ax, %es
    movl %es:0x530, %eax
    movl %eax, 0x490
    movl %es:0x534, %eax
    movl %eax, 0x494
    movl %es:0x538, %eax
    movl %eax, 0x498
    movl %es:0x844, %eax
    movl %eax, 0x49c
    movl %es:0x52c, %eax
    movl %eax, 0x4a0
    movzbl %es:0x5d4, %eax
    movl %eax, 0x4a4
    popl %es
    movw $0x3509, %ax
    pushfl
    popl 0x4c0
    int $0x21
    pushfl
    popl 0x45c
    movl %eax, 0x460
    movl %ebx, 0x464
    movl %ecx, 0x468
    movl %edx, 0x46c
    movl %esi, 0x470
    pushl %es
    movw 0x132a24, %ax
    movw %ax, %es
    movl %es:0x530, %eax
    movl %eax, 0x4a8
    movl %es:0x534, %eax
    movl %eax, 0x4ac
    movl %es:0x538, %eax
    movl %eax, 0x4b0
    movl %es:0x844, %eax
    movl %eax, 0x4b4
    movl %es:0x52c, %eax
    movl %eax, 0x4b8
    movzbl %es:0x5d4, %eax
    movl %eax, 0x4bc
    popl %es
    movl $0x4e0000, %ecx
    movl $0x3508, %eax
    pushfl
    popl 0x4e4
    int $0x21
    pushfl
    popl 0x4cc
    movl %eax, 0x4c4
    movl %ecx, 0x4c8
    pushl %es
    movw 0x132a24, %ax
    movw %ax, %es
    movl %es:0x530, %eax
    movl %eax, 0x4d0
    movl %es:0x534, %eax
    movl %eax, 0x4d4
    movl %es:0x538, %eax
    movl %eax, 0x4d8
    movl %es:0x840, %eax
    movl %eax, 0x4e0
    popl %es
    movl $0x3005, %eax
    movl $0x11223344, %ebx
    movl $0x55667788, %ecx
    movl $0x99aabbcc, %edx
    movl $0x12345678, %esi
    movl $0x89abcdef, %edi
    int $0x21
    movl %eax, 0x474
    movl %ebx, 0x478
    movl %ecx, 0x47c
    movl %edx, 0x480
    movl %esi, 0x484
    movl %edi, 0x488
    pushfl
    popl 0x48c
    movl $0x454e4f44, 0x4fc /* DONE */

    xorl %ecx, %ecx
    movl $0x500, %edx
    movw $0x3c00, %ax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0x100, %ecx
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
.ascii "X32R"
.long 4
.zero 0xf8
.org 0x500
.asciz "X32PROBE.BIN"
