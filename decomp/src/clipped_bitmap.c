/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xca290, 474 bytes. EAX bitmap, EDX clip, BX/CX source X/Y;
 * stack width,height,X,Y,stride; RET20. ESI/EDI/EBP preserved.
 * Callers e276e/e281a supply atlas/packed glyphs; a6ff7 supplies a widget.
 * Keep word wrapping and the original height-only final zero check. */
void blit_bitmap_clipped_ca290(ef_addr32 bitmap, ef_addr32 clip,
                              uint32_t source_x, uint32_t source_y,
                              uint32_t width_arg, uint32_t height_arg,
                              uint32_t x_arg, uint32_t y_arg, uint32_t stride)
{
    uint16_t width = (uint16_t)width_arg, height = (uint16_t)height_arg;
    uint16_t x = (uint16_t)x_arg, y = (uint16_t)y_arg;
    uint16_t sx = (uint16_t)source_x, sy = (uint16_t)source_y;
    int32_t left, top, edge, delta;

    left = EF_S16(clip);
    if ((int32_t)(int16_t)x + (int16_t)width <= left)
        return;
    top = (int32_t)EF_U32(clip) >> 16;
    if ((int32_t)(int16_t)y + (int16_t)height <= top)
        return;
    edge = (int32_t)EF_U32(clip + 2u) >> 16;
    if ((int32_t)(int16_t)x >= left + edge)
        return;
    edge = (int32_t)EF_U32(clip + 4u) >> 16;
    if ((int32_t)(int16_t)y >= top + edge)
        return;

    left = EF_S16(clip);
    if ((int16_t)x < left) {
        delta = left - (int16_t)x;
        width = (uint16_t)(width - delta);
        sx = (uint16_t)(sx + delta);
        x = EF_U16(clip);
    }
    top = EF_S16(clip + 2u);
    if ((int16_t)y < top) {
        delta = top - (int16_t)y;
        height = (uint16_t)(height - delta);
        sy = (uint16_t)(sy + delta);
        y = EF_U16(clip + 2u);
    }
    edge = (int32_t)EF_U32(clip + 2u) >> 16;
    edge += EF_S16(clip);
    if ((int32_t)(int16_t)x + (int16_t)width > edge) {
        width = EF_U16(clip);
        width = (uint16_t)(width + EF_U16(clip + 4u));
        width = (uint16_t)(width - x);
    }
    edge = (int32_t)EF_U32(clip) >> 16;
    edge += (int32_t)EF_U32(clip + 4u) >> 16;
    if ((int32_t)(int16_t)y + (int16_t)height > edge) {
        height = EF_U16(clip + 2u);
        height = (uint16_t)(height + EF_U16(clip + 6u));
        height = (uint16_t)(height - y);
    }
    if (height == 0u)
        return;
    if ((uint16_t)stride == 256u) {
        guest_masked_blit_cd82c(y, x, height, width, sy, sx, bitmap);
    } else {
        packed_blit_cd9a6((uint16_t)stride,
                         (uint32_t)(int32_t)(int16_t)y,
                         (uint32_t)(int32_t)(int16_t)x,
                         (uint32_t)(int32_t)(int16_t)height,
                         (uint32_t)(int32_t)(int16_t)width,
                         (uint32_t)(int32_t)(int16_t)sy,
                         (uint32_t)(int32_t)(int16_t)sx, bitmap);
    }
}
