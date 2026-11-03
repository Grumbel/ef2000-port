/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

/* Read-only measurements of open, device info, read, seek, and close.
 * Seven 32-byte register/flags records, including missing file and console info.
 */
.code32
.section .text
.org 0x86
    movl $0x410, %ebp
    movl $0x12343d00, %eax
    movl $0x530, %edx
    call invoke
    movl %eax, %ebx
    movl $0x12344400, %eax
    movl $0x99aabbcc, %edx
    call invoke
    movl $0x12343f00, %eax
    movl $0x560, %edx
    movl $16, %ecx
    call execute
    movl $0x12344202, %eax
    movl $0x99aa0000, %edx
    movl $0x55660000, %ecx
    call execute
    movl $0x12343e00, %eax
    call invoke
    movl $0x12343d00, %eax
    movl $0x540, %edx
    call invoke
    movl $0x12344400, %eax
    movl $1, %ebx
    movl $0x99aabbcc, %edx
    call invoke
    movl $0x454e4f44, 0x4f0
    xorl %ecx, %ecx
    movl $0x500, %edx
    movw $0x3c00, %ax
    int $0x21
    jc failed
    movzwl %ax, %ebx
    movl $0x400, %edx
    movl $0xf4, %ecx
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
invoke:
    movl $0x55667788, %ecx
execute:
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
.ascii "DFIL"
.long 1
.zero 0xec
.org 0x500
.asciz "DOSFILES.BIN"
.org 0x530
.asciz "DID1ST.DAT"
.org 0x540
.asciz "NOFILE.XYZ"
.org 0x560
.zero 16
