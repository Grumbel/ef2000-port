; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; Convert a double in EDX:EAX to unsigned 64-bit integer EDX:EAX.
; Finite magnitudes below 1 become zero; magnitudes at least 2^64 and
; non-finite values saturate to UINT64_MAX. In-range negative values are
; truncated by magnitude and negated, matching the OpenWatcom helper.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC __FDU87
__FDU87:
    push ecx
    push ebx
    push esi
    mov esi, edx

    mov ecx, edx
    shr ecx, 20
    and ecx, 07ffh
    cmp ecx, 03ffh
    jb short fdu87_zero
    cmp ecx, 043fh
    jae short fdu87_saturate
    sub ecx, 03ffh

    and edx, 000fffffh
    or edx, 00100000h
    cmp ecx, 52
    jb short fdu87_right
    sub ecx, 52
    jz short fdu87_sign
    shld edx, eax, cl
    shl eax, cl
    jmp short fdu87_sign

fdu87_right:
    mov ebx, 52
    sub ebx, ecx
    cmp ebx, 32
    jb short fdu87_right_low
    je short fdu87_right_32
    sub ebx, 32
    mov ecx, ebx
    mov eax, edx
    shr eax, cl
    xor edx, edx
    jmp short fdu87_sign
fdu87_right_32:
    mov eax, edx
    xor edx, edx
    jmp short fdu87_sign
fdu87_right_low:
    mov ecx, ebx
    shrd eax, edx, cl
    shr edx, cl

fdu87_sign:
    test esi, 80000000h
    jz short fdu87_done
    neg edx
    neg eax
    sbb edx, 0
    jmp short fdu87_done

fdu87_zero:
    xor eax, eax
    xor edx, edx
    jmp short fdu87_done
fdu87_saturate:
    mov eax, 0ffffffffh
    mov edx, eax
fdu87_done:
    pop esi
    pop ebx
    pop ecx
    ret
_TEXT ENDS
END
