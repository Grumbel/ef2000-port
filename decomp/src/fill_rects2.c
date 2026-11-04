/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Clipped fills and parameter blocks. */

/* 0xc91c8, 111 bytes. Fill a rectangle given as exclusive coordinates when it lies inside the screen (limits 0x15b744/0x15b748); colours up to 0x10 are remapped through a table at 0x13793c. */
void fill_rect_clipped_c91c8(uint32_t colour, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    uint32_t c = colour & 0xffffu;
    uint32_t width = EF_U32(0x0015b744u);
    uint32_t height = EF_U32(0x0015b748u);
    if ((int32_t)c <= 0x10)
        c = EF_U8(0x0013793cu + c);
    c &= 0xffu;
    x0 &= 0xffffu; y0 &= 0xffffu; x1 &= 0xffffu; y1 &= 0xffffu;
    if ((int32_t)x0 >= (int32_t)width)
        return;
    if ((int32_t)y0 >= (int32_t)height)
        return;
    if ((int32_t)x1 > (int32_t)width)
        return;
    if ((int32_t)y1 > (int32_t)height)
        return;
    fill_rect_ceccc(c, x0, y0, x1 - 1u, y1 - 1u);
}

/* 0xdd9e4, 112 bytes. Store a parameter block at 0x15ee80, bracketing the writes with 0xdd870/0xdd8c0 unless 0x15ee94 is -1. */
void set_param_block_dd9e4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint8_t skip = (EF_S16(0x0015ee94u) == -1);
    if (!skip)
        guest_fn_dd870_x();
    EF_U32(0x0015ee80u) = a;
    EF_U16(0x0015ee88u) = (uint16_t)s0;
    EF_U32(0x0015ee90u) = b;
    EF_U16(0x0015ee8au) = (uint16_t)s1;
    EF_U16(0x0015ee84u) = (uint16_t)c;
    EF_U16(0x0015ee8cu) = (uint16_t)s2;
    EF_U16(0x0015ee86u) = (uint16_t)d;
    EF_U16(0x0015ee8eu) = (uint16_t)s3;
    if (!skip)
        guest_fn_dd8c0_x();
}
