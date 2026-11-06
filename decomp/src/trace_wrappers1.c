/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* AIL trace wrappers. */

/* 0x101ddc, 53 bytes. AIL trace wrapper for 0x104694 (cdecl, all arguments on the stack). */
uint32_t trace_call_101ddc(void)
{
    uint32_t result;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001938e8u);
    result = guest_fn_104694_t();
    if (EF_U32(0x0017d2c8u) == 1u)
        return open_list_node_bc421(EF_U32(0x004dd620u)); /* tail jump into 0xbc421 */
    return result;
}

/* 0x101e14, 92 bytes. AIL trace wrapper for 0x1041e4 with result trace (cdecl, all arguments on the stack). */
uint32_t EF_CDECL trace_call_101e14(uint32_t a0, uint32_t a1)
{
    uint32_t result;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001938f8u, a0, a1);
    result = guest_fn_1041e4_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x101e80, 84 bytes. AIL trace wrapper for 0x111118 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_101e80(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193924u, a0);
    result = guest_fn_111118_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x101f08, 58 bytes. AIL trace wrapper for 0x1111b1 (cdecl, all arguments on the stack). */
uint32_t trace_call_101f08(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193970u, a0, a1, a2);
    return guest_fn_1111b1_t(a0, a1, a2);
}

/* 0x101f88, 106 bytes. AIL trace wrapper for 0x11124c with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_101f88(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001939acu, a0, a1, a2, a3);
    result = guest_fn_11124c_t(a0, a1, a2, a3);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x102144, 84 bytes. AIL trace wrapper for 0x111365 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102144(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a70u, a0);
    result = guest_fn_111365_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a60u, result);
    return result;
}

/* 0x102198, 92 bytes. AIL trace wrapper for 0x1113ac with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102198(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a8cu, a0, a1);
    result = guest_fn_1113ac_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a60u, result);
    return result;
}

/* 0x1023d8, 84 bytes. AIL trace wrapper for 0x104504 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_1023d8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193bbcu, a0);
    result = guest_fn_104504_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x10242c, 92 bytes. AIL trace wrapper for 0x104200 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_10242c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193bdcu, a0, a1);
    result = guest_fn_104200_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x1024b4, 84 bytes. AIL trace wrapper for 0x106514 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_1024b4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193c18u, a0);
    result = guest_fn_106514_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x102508, 92 bytes. AIL trace wrapper for 0x106464 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102508(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193c34u, a0, a1);
    result = guest_fn_106464_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x102590, 84 bytes. AIL trace wrapper for 0x1065c8 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102590(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193c7cu, a0);
    result = guest_fn_1065c8_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x1026fc, 58 bytes. AIL trace wrapper for 0x106718 (cdecl, all arguments on the stack). */
uint32_t trace_call_1026fc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193d28u, a0, a1, a2);
    return guest_fn_106718_t(a0, a1, a2);
}

/* 0x102738, 58 bytes. AIL trace wrapper for 0x106740 (cdecl, all arguments on the stack). */
uint32_t trace_call_102738(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193d50u, a0, a1, a2);
    return guest_fn_106740_t(a0, a1, a2);
}

/* 0x1028f4, 84 bytes. AIL trace wrapper for 0x106708 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_1028f4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193e64u, a0);
    result = guest_fn_106708_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a60u, result);
    return result;
}

/* 0x102dbc, 92 bytes. AIL trace wrapper for 0x106938 with result trace (cdecl, all arguments on the stack). */
uint32_t EF_CDECL trace_call_102dbc(uint32_t a0, uint32_t a1)
{
    uint32_t result;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194074u, a0, a1);
    result = guest_fn_106938_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x102f60, 84 bytes. AIL trace wrapper for 0x113af0 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102f60(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x0019412cu, a0);
    result = guest_fn_113af0_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x102fb4, 92 bytes. AIL trace wrapper for 0x113a40 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_102fb4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194148u, a0, a1);
    result = guest_fn_113a40_t(a0, a1);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x10303c, 84 bytes. AIL trace wrapper for 0x113c7c with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_10303c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194190u, a0);
    result = guest_fn_113c7c_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193940u, result);
    return result;
}

/* 0x1030bc, 95 bytes. AIL trace wrapper for 0x113d10 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_1030bc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001941d8u, a0, a1, a2);
    result = guest_fn_113d10_t(a0, a1, a2);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x1031cc, 58 bytes. AIL trace wrapper for 0x1141b0 (cdecl, all arguments on the stack). */
uint32_t trace_call_1031cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194268u, a0, a1, a2);
    return guest_fn_1141b0_t(a0, a1, a2);
}

/* 0x1034b0, 84 bytes. AIL trace wrapper for 0x113b78 with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_1034b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001943c4u, a0);
    result = guest_fn_113b78_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x103778, 102 bytes. AIL trace wrapper for 0x11451c with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_103778(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001944ecu, a0, a1, a2);
    result = guest_fn_11451c_t(a0, a1, a2);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194514u, result);
    return result;
}

/* 0x103b08, 84 bytes. AIL trace wrapper for 0x1146ec with result trace (cdecl, all arguments on the stack). */
uint32_t trace_call_103b08(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0)
{
    uint32_t result;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001946b0u, a0);
    result = guest_fn_1146ec_t(a0);
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193914u, result);
    return result;
}

/* 0x103c28, 81 bytes. AIL trace wrapper for 0x1149e0 (cdecl, all arguments on the stack). */
uint32_t trace_call_103c28(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194734u, a0, a1, a2, a3, a4);
    return guest_fn_1149e0_t(a0, a1, a2, a3, a4);
}

/* 0x1023b0, 38 bytes. AIL trace wrapper for 0x1113fb (no arguments, tail call). */
uint32_t trace_call_1023b0(void)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193ba0u);
    return guest_fn_1113fb_t();
}
