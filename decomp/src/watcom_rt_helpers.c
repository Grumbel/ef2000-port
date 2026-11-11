/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Compiler runtime helpers that the OpenWatcom 1.9 code generator calls but
 * the original game (an older compiler) inlined: 64-bit shifts, multiply and
 * divide, and the truncating float-to-integer rounding step. They are
 * register-convention routines (A = EDX:EAX, B = ECX:EBX, count in EBX) that
 * preserve every register except the result registers (the aux pragmas below declare everything modified so that the compiler emits no prologue; the asm saves what it uses).
 *
 * The bodies are hand-written assembly; they are not recovered game code. */

#include "ef_abi.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)

void __U8LS(void);
void __U8RS(void);
void __I8RS(void);
void __U8M(void);
void __I8M(void);
void __U8D(void);
void __I8D(void);
void __CHP(void);

#pragma aux __U8LS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8RS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8RS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8M "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8M "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8D "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8D "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __CHP "*" parm [] modify [eax ebx ecx edx esi edi ebp]

/* EDX:EAX <<= EBX (counts of 64 or more give 0). */
void __U8LS(void)
{
    _asm {
        push ecx
        mov ecx, ebx
        cmp ecx, 64
        jae u8ls_zero
        cmp cl, 32
        jb u8ls_small
        mov edx, eax
        xor eax, eax
        sub cl, 32
        shl edx, cl
        jmp u8ls_done
    u8ls_small:
        shld edx, eax, cl
        shl eax, cl
        jmp u8ls_done
    u8ls_zero:
        xor eax, eax
        xor edx, edx
    u8ls_done:
        pop ecx
        ret
    }
}

/* EDX:EAX >>= EBX, logical. */
void __U8RS(void)
{
    _asm {
        push ecx
        mov ecx, ebx
        cmp ecx, 64
        jae u8rs_zero
        cmp cl, 32
        jb u8rs_small
        mov eax, edx
        xor edx, edx
        sub cl, 32
        shr eax, cl
        jmp u8rs_done
    u8rs_small:
        shrd eax, edx, cl
        shr edx, cl
        jmp u8rs_done
    u8rs_zero:
        xor eax, eax
        xor edx, edx
    u8rs_done:
        pop ecx
        ret
    }
}

/* EDX:EAX >>= EBX, arithmetic. */
void __I8RS(void)
{
    _asm {
        push ecx
        mov ecx, ebx
        cmp ecx, 63
        jbe i8rs_ok
        mov ecx, 63
    i8rs_ok:
        cmp cl, 32
        jb i8rs_small
        mov eax, edx
        sar edx, 31
        sub cl, 32
        sar eax, cl
        jmp i8rs_done
    i8rs_small:
        shrd eax, edx, cl
        sar edx, cl
    i8rs_done:
        pop ecx
        ret
    }
}

/* EDX:EAX *= ECX:EBX (low 64 bits, identical for signed and unsigned). */
void __U8M(void)
{
    _asm {
        push ecx
        imul edx, ebx
        imul ecx, eax
        add ecx, edx
        mul ebx
        add edx, ecx
        pop ecx
        ret
    }
}

void __I8M(void)
{
    _asm {
        push ecx
        imul edx, ebx
        imul ecx, eax
        add ecx, edx
        mul ebx
        add edx, ecx
        pop ecx
        ret
    }
}

/* EDX:EAX / ECX:EBX unsigned: quotient in EDX:EAX, remainder in ECX:EBX. */
void __U8D(void)
{
    _asm {
        push ebp
        push esi
        push edi
        xor esi, esi
        xor edi, edi
        mov ebp, 64
    u8d_loop:
        shl eax, 1
        rcl edx, 1
        rcl edi, 1
        rcl esi, 1
        jc u8d_sub
        cmp esi, ecx
        jb u8d_next
        ja u8d_sub
        cmp edi, ebx
        jb u8d_next
    u8d_sub:
        sub edi, ebx
        sbb esi, ecx
        or eax, 1
    u8d_next:
        dec ebp
        jnz u8d_loop
        mov ebx, edi
        mov ecx, esi
        pop edi
        pop esi
        pop ebp
        ret
    }
}

/* Signed divide: the quotient is negative when the signs differ, the
 * remainder takes the sign of the dividend. */
void __I8D(void)
{
    _asm {
        push ebp
        push esi
        xor ebp, ebp
        mov esi, edx
        test edx, edx
        jns i8d_a_ok
        not edx
        neg eax
        sbb edx, -1
    i8d_a_ok:
        test ecx, ecx
        jns i8d_b_ok
        not ecx
        neg ebx
        sbb ecx, -1
        xor ebp, 1
    i8d_b_ok:
        test esi, esi
        jns i8d_go
        xor ebp, 3
    i8d_go:
        call __U8D
        test ebp, 1
        jz i8d_q_ok
        not edx
        neg eax
        sbb edx, -1
    i8d_q_ok:
        test ebp, 2
        jz i8d_r_ok
        not ecx
        neg ebx
        sbb ecx, -1
    i8d_r_ok:
        pop esi
        pop ebp
        ret
    }
}

/* ST(0) = trunc(ST(0)) without disturbing the caller's control word. */
void __CHP(void)
{
    _asm {
        push eax
        push eax
        fstcw word ptr [esp]
        mov ax, word ptr [esp]
        or ax, 0c00h
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        frndint
        fldcw word ptr [esp]
        add esp, 4
        pop eax
        ret
    }
}

#endif
