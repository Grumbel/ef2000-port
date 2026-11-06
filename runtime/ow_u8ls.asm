; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom 32-bit unsigned 64-bit left-shift helper used by mechanically
; compiled guest modules. Input/output value is EDX:EAX; count is EBX modulo 64.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __U8LS
__U8LS:
    mov ecx, ebx
    and ecx, 03fh
    jz short u8ls_done
u8ls_loop:
    shl eax, 1
    rcl edx, 1
    dec ecx
    jnz short u8ls_loop
u8ls_done:
    ret
_TEXT ENDS
END
