; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom signed 64-bit divide/modulo helper.
; Input: dividend EDX:EAX, divisor ECX:EBX.
; Output: quotient EDX:EAX, remainder ECX:EBX.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __I8D
__I8D:
    push ebp
    mov ebp, esp
    push esi
    sub esp, 8
    mov dword ptr [ebp-8], 0
    mov dword ptr [ebp-12], 0

    test edx, 80000000h
    jz short i8d_dividend_ready
    neg edx
    neg eax
    sbb edx, 0
    mov dword ptr [ebp-8], 1
i8d_dividend_ready:
    test ecx, 80000000h
    jz short i8d_divisor_ready
    neg ecx
    neg ebx
    sbb ecx, 0
    mov dword ptr [ebp-12], 1
i8d_divisor_ready:
    call i8d_unsigned

    mov esi, dword ptr [ebp-8]
    test esi, esi
    jz short i8d_remainder_ready
    neg ecx
    neg ebx
    sbb ecx, 0
i8d_remainder_ready:
    mov esi, dword ptr [ebp-8]
    xor esi, dword ptr [ebp-12]
    test esi, esi
    jz short i8d_quotient_ready
    neg edx
    neg eax
    sbb edx, 0
i8d_quotient_ready:
    mov esi, dword ptr [ebp-4]
    mov esp, ebp
    pop ebp
    ret

; Restoring unsigned divide. The quotient shifts in the dividend bits while
; EDI:ESI accumulates the remainder; a 65th carry forces subtraction.
i8d_unsigned:
    push ebp
    push esi
    push edi
    xor esi, esi
    xor edi, edi
    mov ebp, 64
i8d_loop:
    shl eax, 1
    rcl edx, 1
    rcl esi, 1
    rcl edi, 1
    jc short i8d_subtract
    cmp edi, ecx
    ja short i8d_subtract
    jb short i8d_next
    cmp esi, ebx
    jb short i8d_next
i8d_subtract:
    sub esi, ebx
    sbb edi, ecx
    or eax, 1
i8d_next:
    dec ebp
    jnz short i8d_loop
    mov ebx, esi
    mov ecx, edi
    pop edi
    pop esi
    pop ebp
    ret
_TEXT ENDS
END
