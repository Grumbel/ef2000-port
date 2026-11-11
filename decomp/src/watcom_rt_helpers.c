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
void __I8LS(void);
void __U8RS(void);
void __I8RS(void);
void __U8M(void);
void __I8M(void);
void __U8D(void);
void __I8D(void);
void __CHP(void);
void if_dsqrt(void);
void if_dsin(void);
void if_dcos(void);
void if_dlog(void);
void if_dtan(void);
void if_datan(void);
void if_dfmod(void);
void if_datan2(void);
void thunk_guest_fatal_arg(void);
void thunk_guest_fatal_arg3(void);
void thunk_guest_report_parser_enum_error(void);
void thunk_guest_parser_line_overflow(void);
void thunk_report_buffer_capacity_exceeded(void);
void thunk_guest_allocate_parser_text(void);
void thunk_read_tactical_indicator_callback(void);
void thunk_guest_callptr4s2_x(void);
void ceil(void);
void floor(void);
void trunc(void);
void *memset(void *dest, int value, unsigned count);
void *memcpy(void *dest, const void *src, unsigned count);

#pragma aux __U8LS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8LS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8RS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8RS "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8M "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8M "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __U8D "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __I8D "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux __CHP "*" parm [] modify [eax ebx ecx edx esi edi ebp]
#pragma aux if_dsqrt "IF@DSQRT" parm [] modify []
#pragma aux if_dsin "IF@DSIN" parm [] modify []
#pragma aux if_dcos "IF@DCOS" parm [] modify []
#pragma aux if_dlog "IF@DLOG" parm [] modify []
#pragma aux if_dtan "IF@DTAN" parm [] modify []
#pragma aux if_datan "IF@DATAN" parm [] modify []
#pragma aux if_dfmod "IF@DFMOD" parm [] modify []
#pragma aux thunk_guest_fatal_arg "guest_fatal_arg_" parm [] modify []
#pragma aux thunk_guest_fatal_arg3 "guest_fatal_arg3_" parm [] modify []
#pragma aux thunk_guest_report_parser_enum_error "guest_report_parser_enum_error_" parm [] modify []
#pragma aux thunk_guest_parser_line_overflow "guest_parser_line_overflow_" parm [] modify []
#pragma aux thunk_report_buffer_capacity_exceeded "report_buffer_capacity_exceeded_" parm [] modify []
#pragma aux thunk_guest_allocate_parser_text "guest_allocate_parser_text_" parm [] modify []
#pragma aux thunk_read_tactical_indicator_callback "read_tactical_indicator_callback_" parm [] modify []
#pragma aux thunk_guest_callptr4s2_x "guest_callptr4s2_x_" parm [] modify []
#pragma aux if_datan2 "IF@DATAN2" parm [] modify []
#pragma aux ceil "*" parm [] modify []
#pragma aux floor "*" parm [] modify []
#pragma aux trunc "*" parm [] modify []

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

/* Signed 64-bit left shift is the same operation as the unsigned one. */
void __I8LS(void)
{
    _asm {
        push ecx
        mov ecx, ebx
        cmp ecx, 64
        jae i8ls_zero
        cmp cl, 32
        jb i8ls_small
        mov edx, eax
        xor eax, eax
        sub cl, 32
        shl edx, cl
        jmp i8ls_done
    i8ls_small:
        shld edx, eax, cl
        shl eax, cl
        jmp i8ls_done
    i8ls_zero:
        xor eax, eax
        xor edx, edx
    i8ls_done:
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

/* Inline-8087 math entries: argument and result in ST0. */
void if_dsqrt(void)
{
    _asm {
        fsqrt
        ret
    }
}

void if_dsin(void)
{
    _asm {
        fsin
        ret
    }
}

void if_dcos(void)
{
    _asm {
        fcos
        ret
    }
}

void if_dlog(void)
{
    _asm {
        fldln2
        fxch st(1)
        fyl2x
        ret
    }
}

void if_dtan(void)
{
    _asm {
        fptan
        fstp st(0)
        ret
    }
}

void if_datan(void)
{
    _asm {
        fld1
        fpatan
        ret
    }
}

/* ST1 = fmod(ST1, ST0), one stack entry popped. */
void if_dfmod(void)
{
    _asm {
        push eax
        fxch st(1)
    again:
        fprem
        fnstsw ax
        sahf
        jp again
        fstp st(1)
        pop eax
        ret
    }
}

/* ceil/floor/trunc: double argument on the stack (callee pops), result ST0. */
void ceil(void)
{
    _asm {
        push eax
        sub esp, 4
        fld qword ptr [esp + 12]
        fstcw word ptr [esp]
        mov ax, word ptr [esp]
        and ax, 0f3ffh
        or ax, 0800h
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        frndint
        fldcw word ptr [esp]
        add esp, 4
        pop eax
        ret 8
    }
}

void floor(void)
{
    _asm {
        push eax
        sub esp, 4
        fld qword ptr [esp + 12]
        fstcw word ptr [esp]
        mov ax, word ptr [esp]
        and ax, 0f3ffh
        or ax, 0400h
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        frndint
        fldcw word ptr [esp]
        add esp, 4
        pop eax
        ret 8
    }
}

void trunc(void)
{
    _asm {
        push eax
        sub esp, 4
        fld qword ptr [esp + 12]
        fstcw word ptr [esp]
        mov ax, word ptr [esp]
        and ax, 0f3ffh
        or ax, 0c00h
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        frndint
        fldcw word ptr [esp]
        add esp, 4
        pop eax
        ret 8
    }
}

/* ST1 = atan2(ST1, ST0), one entry popped. */
void if_datan2(void)
{
    _asm {
        fpatan
        ret
    }
}

/* Register-convention entries for original indirect cdecl services: the
 * recovered callers use the default register convention, the original code
 * pushes the arguments (format last) and cleans up. */
/* format EAX, value EDX; cdecl via [0x132a74] */
void thunk_guest_fatal_arg(void)
{
    _asm {
        push edx
        push eax
        call dword ptr ds:[0x132a74]
        add esp, 8
        ret
    }
}

/* format EAX, a EDX, b EBX, c ECX; cdecl via [0x132a74] */
void thunk_guest_fatal_arg3(void)
{
    _asm {
        push ecx
        push ebx
        push edx
        push eax
        call dword ptr ds:[0x132a74]
        add esp, 16
        ret
    }
}

/* format EAX, token EDX, context EBX; cdecl via [0x132a74] */
void thunk_guest_report_parser_enum_error(void)
{
    _asm {
        push ebx
        push edx
        push eax
        call dword ptr ds:[0x132a74]
        add esp, 12
        ret
    }
}

/* message EAX; cdecl via [0x132a74] */
void thunk_guest_parser_line_overflow(void)
{
    _asm {
        push eax
        call dword ptr ds:[0x132a74]
        add esp, 4
        ret
    }
}

/* message EAX; cdecl via [0x132a74] */
void thunk_report_buffer_capacity_exceeded(void)
{
    _asm {
        push eax
        call dword ptr ds:[0x132a74]
        add esp, 4
        ret
    }
}

/* Same register convention as the original indirect allocator [0x121ebc]
 * (bytes EAX, source EDX) and tactical callback [0x15bab8]: tail jumps. */
void thunk_guest_allocate_parser_text(void)
{
    _asm {
        jmp dword ptr ds:[0x121ebc]
    }
}

void thunk_read_tactical_indicator_callback(void)
{
    _asm {
        jmp dword ptr ds:[0x15bab8]
    }
}

/* fn EAX, a EDX, b EBX, c ECX, d/s0/s1 on the stack (callee pops 12). The
 * target takes a,b,c,d in EAX,EDX,EBX,ECX and s0,s1 on the stack (callee pops 8). */
void thunk_guest_callptr4s2_x(void)
{
    _asm {
        push esi
        mov esi, eax
        mov eax, edx
        mov edx, ebx
        mov ebx, ecx
        mov ecx, dword ptr [esp + 8]
        push dword ptr [esp + 16]
        push dword ptr [esp + 16]
        call esi
        pop esi
        ret 12
    }
}

/* Minimal C runtime pieces the hand-written host-style code in the hybrid
 * calls through the Watcom default register convention (memset_/memcpy_). */
void *memset(void *dest, int value, unsigned count)
{
    unsigned char *p = (unsigned char *)dest;

    while (count--)
        *p++ = (unsigned char)value;
    return dest;
}

void *memcpy(void *dest, const void *src, unsigned count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    while (count--)
        *d++ = *s++;
    return dest;
}

#endif
