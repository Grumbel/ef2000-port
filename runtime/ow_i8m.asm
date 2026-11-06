; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom 32-bit signed 64-bit multiply helper used by mechanically
; compiled guest modules. Operands are EDX:EAX and ECX:EBX; result is EDX:EAX.
; The low 64 product is identical for signed and unsigned two's-complement
; operands, so only 32-bit partial products are needed.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __I8M
__I8M:
    push ebx
    push ecx
    push esi
    push edi
    push ebp
    mov esi, eax
    mov edi, edx
    mul ebx
    push eax
    mov ebp, edx
    mov eax, edi
    imul eax, ebx
    add ebp, eax
    mov eax, esi
    imul eax, ecx
    add ebp, eax
    mov edx, ebp
    pop eax
    pop ebp
    pop edi
    pop esi
    pop ecx
    pop ebx
    ret
_TEXT ENDS
END
