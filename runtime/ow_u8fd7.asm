; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom helper: convert unsigned EDX:EAX to double EDX:EAX.
; Convert each unsigned half through a signed 32-bit x87 load, correct
; negative halves by 2^32, combine exactly in 80-bit precision, then store
; once as a double using the caller's rounding mode.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __U8FD7
__U8FD7:
    push eax
    push edx
    sub esp, 4
    fnstcw word ptr [esp]
    mov ax, word ptr [esp]
    and ax, 0fcffh
    or ax, 0300h
    mov word ptr [esp+2], ax
    fldcw word ptr [esp+2]

    fild dword ptr [esp+4]
    test dword ptr [esp+4], 80000000h
    jz short u8fd7_hi_ready
    fadd qword ptr u8fd7_two32
u8fd7_hi_ready:
    fmul qword ptr u8fd7_two32

    fild dword ptr [esp+8]
    test dword ptr [esp+8], 80000000h
    jz short u8fd7_lo_ready
    fadd qword ptr u8fd7_two32
u8fd7_lo_ready:
    faddp st(1), st(0)
    fstp qword ptr [esp+4]
    fldcw word ptr [esp]
    mov eax, dword ptr [esp+4]
    mov edx, dword ptr [esp+8]
    add esp, 12
    ret
_TEXT ENDS

_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
u8fd7_two32 DQ 041f0000000000000h
_DATA ENDS
END
