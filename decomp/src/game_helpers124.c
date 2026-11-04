/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* View volume test and range registration. */

/* 0xceb40, 183 bytes. Test a point (z, y, x on the stack after a margin) against the five planes at 0x15b79c: returns 1 when a coordinate is outside +-0x40000000 or the point is more than margin outside a plane, otherwise 0; stores the smallest remaining distance at 0x4208a8 (cdecl, 4 stack args). */
uint32_t point_in_view_volume_ceb40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, int32_t margin, int32_t z, int32_t y, int32_t x)
{
    int32_t negmargin = -margin;
    uint32_t i;
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_S32(0x004208a8u) = 0x7d00;
    if (z > 0x40000000 || z < (int32_t)0xc0000000)
        return 1;
    if (x > 0x40000000 || x < (int32_t)0xc0000000)
        return 1;
    if (y > 0x40000000 || y < (int32_t)0xc0000000)
        return 1;
    for (i = 0; i < 0x28u; i += 8u) {
        int32_t dist = (int32_t)plane_eval_hi_ce4dc(EF_U32(0x0015b79cu + i), (int32_t)x, (int32_t)y, (int32_t)z);
        if (dist < negmargin)
            return 1;
        dist += negmargin;
        if (dist < EF_S32(0x004208a8u))
            EF_S32(0x004208a8u) = dist;
    }
    return 0;
}

/* 0x111575, 37 bytes. Register the two code ranges 0x17d624..0x17e1d4 and 0x110dcc..0x111575 with the stack range tracker. */
void register_code_ranges_111575(void)
{
    guest_fn_104840(0x0017d624u, 0x0017e1d4u);
    guest_fn_104840(0x00110dccu, 0x00111575u);
}
