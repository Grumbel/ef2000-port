; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom x87 chop helper. Round ST(0) toward zero and preserve the
; caller's x87 control word, integer registers, and EFLAGS.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __CHP
__CHP:
    pushfd
    push eax
    sub esp, 4
    fnstcw word ptr [esp]
    mov ax, word ptr [esp]
    and ax, 0f3ffh
    or ax, 0c00h
    mov word ptr [esp+2], ax
    fldcw word ptr [esp+2]
    frndint
    fldcw word ptr [esp]
    add esp, 4
    pop eax
    popfd
    ret
_TEXT ENDS
END
