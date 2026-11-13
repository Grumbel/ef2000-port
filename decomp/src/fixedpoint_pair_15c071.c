/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 64-bit divide/multiply by single x86 instructions: the compiler's own
 * 64-bit helpers would call runtime code that is not part of the hybrid arena. */
/* edx:eax = 1:0 divided by d (idiv), quotient in eax. */
#pragma aux recip_2p32 = "xor eax,eax" "mov edx,1" "idiv ecx" parm [ecx] value [eax] modify exact [eax edx]
extern int32_t recip_2p32(int32_t d);
/* High 32 bits of the signed 64-bit product a*b (imul, edx). */
#pragma aux imul_high = "imul edx" parm [eax] [edx] value [edx] modify exact [eax edx]
extern int32_t imul_high(int32_t a, int32_t b);

/* 0x15c071, 83 bytes. Project a pair of 24.8 fixed-point offsets (x, y) by
 * 1/(depth>>16) and store them, biased by the screen centre words at
 * 0x15bdb8/0x15bdbc, as two 16-bit values at out. Depth outside 16..20000
 * stores 0xffffffff and returns 0. */
#pragma aux write_projected_fixedpoint_pair parm [ebx] [ecx] [esi] [edi] value [eax] modify [eax ecx edx esi ebp]
uint32_t write_projected_fixedpoint_pair(ef_addr32 out, int32_t depth, int32_t x, int32_t y)
{
    int32_t d = depth >> 16;
    int32_t inv;
    if (d < 16 || d > 20000) {
        EF_U32(out) = 0xffffffffu;
        return 0u;
    }
    inv = recip_2p32(d);
    EF_U16(out) = (uint16_t)((uint32_t)imul_high(x >> 8, inv) + EF_U16(0x0015bdb8u));
    EF_U16(out + 2u) = (uint16_t)((uint32_t)imul_high(y >> 8, inv) + EF_U16(0x0015bdbcu));
    return 1u;
}
