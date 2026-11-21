/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define CLIP_RECT          0x000fd518u /* ESI = x0,y0,x1,y1 words, carry when fully clipped */
#define DRAW_SIX_RECT      0x0010d188u /* ESI = clipped rectangle */
#define FIVE_RECT          0x004208e4u
#define SIX_RECT           0x00159088u

/* 0xc8ccc, 58 bytes. Store five stack words (callee pops) into the packed
 * block at 0x4208e4 in reverse order; the last four, from 0x4208e6, are a
 * rectangle that 0xfd518 clips in place. Unless it is entirely outside,
 * 0xcf78c draws the block. */
void renderer_viewport_five_c8ccc(uint16_t a, uint16_t b, uint16_t c,
                                 uint16_t d, uint16_t e)
{
    EF_U16(FIVE_RECT) = e;
    EF_U16(FIVE_RECT + 2u) = d;
    EF_U16(FIVE_RECT + 4u) = c;
    EF_U16(FIVE_RECT + 6u) = b;
    EF_U16(FIVE_RECT + 8u) = a;
    if (!guest_callptr_esi_carry_x(CLIP_RECT, FIVE_RECT + 2u))
        forward_record_cf78c();
}

/* 0xe7a34, 71 bytes. Six stack words (callee pops) into 0x159088: the fifth
 * and sixth first, then the fourth down to the first. The last four, from
 * 0x15908c, are the rectangle clipped by 0xfd518 and drawn by 0x10d188. */
void renderer_viewport_six_e7a34(uint16_t a, uint16_t b, uint16_t c,
                                uint16_t d, uint16_t e, uint16_t f)
{
    EF_U16(SIX_RECT) = e;
    EF_U16(SIX_RECT + 2u) = f;
    EF_U16(SIX_RECT + 4u) = d;
    EF_U16(SIX_RECT + 6u) = c;
    EF_U16(SIX_RECT + 8u) = b;
    EF_U16(SIX_RECT + 10u) = a;
    if (!guest_callptr_esi_carry_x(CLIP_RECT, SIX_RECT + 4u))
        guest_callptr_esi_x(DRAW_SIX_RECT, SIX_RECT + 4u);
}
