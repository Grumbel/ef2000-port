; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom signed 64-bit arithmetic right shift.
; Input/output: EDX:EAX value; EBX shift count, reduced modulo 64.
; ECX is the volatile loop counter.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __I8RS
__I8RS:
    mov ecx, ebx
    and ecx, 03fh
    jz short i8rs_done
i8rs_loop:
    sar edx, 1
    rcr eax, 1
    dec ecx
    jnz short i8rs_loop
i8rs_done:
    ret
_TEXT ENDS
END
