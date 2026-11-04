/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tactical map markers. */

/* 0x5d48c, 148 bytes. Draw a diamond of four lines around (x, y); the half-size is the word at 0x1a6ce8 shifted by the zoom byte 0x12b369 and the colour is 0x128148. */
void draw_diamond_5d48c(uint32_t x, uint32_t y)
{
    int32_t s = (int16_t)(uint16_t)((uint32_t)EF_U16(0x001a6ce8u) << EF_U8(0x0012b369u));
    int32_t cx = (int16_t)x, cy = (int16_t)y;
    uint32_t colour = EF_U8(0x00128148u);
    draw_tactical_line(cy - s, cx, cy, cx - s, colour);
    draw_tactical_line(cy, cx + s, cy - s, cx, colour);
    draw_tactical_line(cy + s, cx, cy, cx + s, colour);
    draw_tactical_line(cy, cx - s, cy + s, cx, colour);
}
