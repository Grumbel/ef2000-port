/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Framebuffer fills. */

/* 0xceccc, 99 bytes. Fill the rectangle x0..x1, y0..y1 (coordinates swapped into order) with a colour byte, one row at a time. Callee pops the fifth argument. */
void fill_rect_ceccc(uint32_t colour, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t dst;
    uint32_t width;
    int32_t t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)y0 * 4u) + (uint32_t)x0;
    width = (uint32_t)(x1 - x0 + 1);
    while (y0 <= y1) {
        fill_bytes_b3fb0(dst, (uint8_t)colour, width);
        y0++;
        dst += EF_U32(0x001631f4u);
    }
}
