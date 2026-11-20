/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

static ef_addr32 raster_row(int16_t row)
{
    return g_raster_framebuffer + EF_U32(0x00163200u + (uint32_t)((int32_t)row * 4));
}

/* One diamond row starting at column COL: clipped to the draw rectangle,
 * it covers COL .. COL + WIDTH - 2 (one pixel less than the unclipped
 * fill draws). */
static void fill_clipped_row(uint8_t color, int16_t row, int16_t col, int16_t width)
{
    int16_t left = col;
    int16_t right = (int16_t)(col + width - 1);
    ef_addr32 p;
    int16_t x;

    if ((int32_t)row < g_draw_clip_top || (int32_t)row > g_draw_clip_bottom)
        return;
    if ((int32_t)col < g_draw_clip_left)
        left = (int16_t)g_draw_clip_left;
    if ((int32_t)right > g_draw_clip_right)
        right = (int16_t)g_draw_clip_right;
    if (right < left)
        return;
    p = raster_row(row) + (uint32_t)(int32_t)left;
    for (x = left; x < right; ++x)
        EF_U8(p++) = color;
}

/* 0xe2c40, 546 bytes. AL colour, DX/BX centre, CX radius (words). Fill a
 * diamond into the frame buffer, clipped to the draw rectangle
 * 0x17cf9c..0x17cfa8. Nothing is drawn when it lies wholly outside; when
 * it lies wholly inside the unclipped fill 0xe2ba0 draws it. EBX, ECX and
 * EDX are clobbered. */
void fill_clipped_diamond_e2c40(uint32_t color, int32_t x_in, int32_t y_in, int32_t radius_in)
{
    int16_t x = (int16_t)x_in;
    int16_t y = (int16_t)y_in;
    int32_t r = (int16_t)radius_in;
    int16_t col = x;
    int16_t row = (int16_t)(y - r);
    int16_t width = 1;

    if (x + r < g_draw_clip_left || x - r > g_draw_clip_right ||
        y + r < g_draw_clip_top || y - r > g_draw_clip_bottom)
        return;
    if (x - r >= g_draw_clip_left && x + r <= g_draw_clip_right &&
        y - r >= g_draw_clip_top && y + r <= g_draw_clip_bottom) {
        fill_diamond_e2ba0(color & 0xffu, (uint32_t)(int32_t)x, (uint32_t)(int32_t)y, (uint32_t)r);
        return;
    }
    for (; width < 2 * r; width = (int16_t)(width + 2), --col, ++row)
        fill_clipped_row((uint8_t)color, row, col, width);
    for (; width > 0; width = (int16_t)(width - 2), ++col, ++row)
        fill_clipped_row((uint8_t)color, row, col, width);
}
