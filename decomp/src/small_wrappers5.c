/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Framebuffer row transfers. */

static void screen_rows_common(uint32_t x, uint32_t y, uint32_t width, uint32_t rows, ef_addr32 buf, int to_screen)
{
    uint32_t dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (y & 0xffffu) * 4u) + (x & 0xffffu);
    uint32_t n = rows;
    if ((uint16_t)n == 0u)
        return;
    do {
        if (to_screen)
            guest_fn_e9a24(width & 0xffffu, buf, dst);
        else
            guest_fn_e9a24(width & 0xffffu, dst, buf);
        dst += EF_U32(0x001631f4u);
        n--;
        buf += width & 0xffffu;
    } while ((uint16_t)n > 0u);
}

/* 0xeb050, 94 bytes. Copy a packed buffer of rows to the framebuffer at (x, y). */
void put_screen_rows_eb050(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, ef_addr32 buf)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    screen_rows_common(x, y, width, rows, buf, 1);
}

/* 0xeb1f0, 94 bytes. Copy rows from the framebuffer at (x, y) into a packed buffer. */
void get_screen_rows_eb1f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, ef_addr32 buf)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    screen_rows_common(x, y, width, rows, buf, 0);
}
