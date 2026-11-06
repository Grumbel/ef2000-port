; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom 32-bit unsigned 64-bit right-shift helper used by mechanically
; compiled guest modules. Input/output value is EDX:EAX; count is EBX modulo 64.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __U8RS
__U8RS:
    mov ecx, ebx
    and ecx, 03fh
    jz short u8rs_done
u8rs_loop:
    shr edx, 1
    rcr eax, 1
    dec ecx
    jnz short u8rs_loop
u8rs_done:
    ret
_TEXT ENDS
END
