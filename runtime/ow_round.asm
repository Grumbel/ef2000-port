; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; Ghidra FLOAT_ROUND bridge used by mechanically compiled guest modules.
; C ABI: double ROUND(double), argument at [ESP+4], result in ST(0).
; Round to the nearest integral value, with halfway values away from zero,
; independent of the caller's x87 rounding mode. Preserve that mode.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC ROUND_
ROUND_:
    push ebp
    mov ebp, esp
    sub esp, 4
    fnstcw word ptr [esp]
    mov ax, word ptr [esp]
    and ax, 0f0ffh
    or ax, 0f00h
    mov word ptr [esp+2], ax
    fldcw word ptr [esp+2]
    fld qword ptr [ebp+8]
    test dword ptr [ebp+12], 80000000h
    jnz short round_negative
    fadd qword ptr round_half
    jmp short round_integral
round_negative:
    fsub qword ptr round_half
round_integral:
    frndint
    fldcw word ptr [esp]
    mov esp, ebp
    pop ebp
    ret
_TEXT ENDS

_DATA SEGMENT DWORD PUBLIC USE32 'DATA'
round_half DQ 03fe0000000000000h
_DATA ENDS
END
