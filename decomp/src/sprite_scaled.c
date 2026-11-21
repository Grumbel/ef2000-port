/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define SPRITE_CLIP_RECT 0x0041ec08u

/* 0xca620, 349 bytes. Draw the WIDTH x HEIGHT bitmap at (X, Y) scaled into
 * the current view (dword 0x41eea0): each coordinate is doubled, offset by
 * the size, multiplied by the view scale (double +0x9f) and the constant
 * double at 0x18f2e3, re-centred by half the size and moved to the view
 * origin, then truncated with 0x9d72c. Nothing is drawn while byte 0x41f2e5
 * is clear or when the sprite lies outside the clip rectangle at 0x41ec08;
 * otherwise 0xca290 blits it with STRIDE. Returns the CX the original
 * leaves: the origin Y without drawing, the width when clipped, else the
 * blitter's CX (0x4d24c's caller reads it as a default count). RET 8. */
uint16_t draw_scaled_sprite_ca620(ef_addr32 bitmap, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t stride)
{
    ef_addr32 view = EF_U32(0x0041eea0u);
    int16_t origin_x = (int16_t)(EF_U16(view + 0x41u) + EF_U16(view + 0x71u) - EF_U16(view + 0x69u));
    int16_t origin_y = (int16_t)(EF_U16(view + 0x43u) + EF_U16(view + 0x73u) - EF_U16(view + 0x6bu));
    int32_t w = (int16_t)width;
    int32_t h = (int16_t)height;
    double scale, factor;
    int16_t sx, sy;
    int32_t left, top;

    if (EF_U8(0x0041f2e5u) == 0u)
        return (uint16_t)origin_y;
    scale = EF_F64(view + 0x9fu);
    factor = EF_F64(0x0018f2e3u);
    sx = (int16_t)(int32_t)truncate_9d72c((double)(2 * (int32_t)(int16_t)x + w) * scale * factor
                                          - (double)(w / 2) + (double)origin_x);
    sy = (int16_t)(int32_t)truncate_9d72c((double)(2 * (int32_t)(int16_t)y + h) * scale * factor
                                          - (double)(h / 2) + (double)origin_y);
    left = (int16_t)EF_U16(SPRITE_CLIP_RECT);
    top = (int16_t)EF_U16(SPRITE_CLIP_RECT + 2u);
    if (sx >= left + (int16_t)EF_U16(SPRITE_CLIP_RECT + 4u))
        return (uint16_t)w;
    if (sy >= top + (int16_t)EF_U16(SPRITE_CLIP_RECT + 6u))
        return (uint16_t)w;
    if (sx + w < left || sy + h < top)
        return (uint16_t)w;
    return guest_fn_ca290_cx(bitmap, SPRITE_CLIP_RECT, 0u, 0u, (uint32_t)w, (uint32_t)h,
                             (uint32_t)(int32_t)sx, (uint32_t)(int32_t)sy, (uint32_t)(uint16_t)stride);
}
