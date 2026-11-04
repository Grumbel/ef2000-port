/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Four near-identical table-driven box drawing routines. */

/* Draw one row of three boxes per table entry 1..count: colours 3, 4 and 1, each from a (low, high) word pair at table + 0x28 * k, offset by 100, at a position that advances by 10 in x and y per entry. The entry count is the word at limit + 2. */
static void draw_box_rows_common(uint32_t table, uint32_t limit)
{
    int16_t x = 0;
    int16_t y = 10;
    int16_t index = 1;

    while (index <= EF_S16(limit + 2u)) {
        uint32_t slot = (uint32_t)(int32_t)index * 2u;
        int32_t lo;
        int32_t hi;

        lo = (int16_t)(EF_S16(table + slot) + 0x64);
        hi = (int16_t)(EF_S16(table + 2u + slot) + 0x64);
        guest_fn_c95ac_x(3u, (uint32_t)(int32_t)x, (uint32_t)lo, (uint32_t)(int32_t)y, (uint32_t)hi);
        lo = (int16_t)(EF_S16(table + 0x28u + slot) + 0x64);
        hi = (int16_t)(EF_S16(table + 0x2au + slot) + 0x64);
        guest_fn_c95ac_x(4u, (uint32_t)(int32_t)x, (uint32_t)lo, (uint32_t)(int32_t)y, (uint32_t)hi);
        lo = (int16_t)(EF_S16(table + 0x50u + slot) + 0x64);
        hi = (int16_t)(EF_S16(table + 0x52u + slot) + 0x64);
        guest_fn_c95ac_x(1u, (uint32_t)(int32_t)x, (uint32_t)lo, (uint32_t)(int32_t)y, (uint32_t)hi);
        y = (int16_t)(y + 10);
        x = (int16_t)(x + 10);
        index++;
    }
}

/* 0x99ee4, 251 bytes. Draw the box rows for the table at 0x40cdf6 (count word at 0x40d02a). */
void draw_box_rows_99ee4(void)
{
    draw_box_rows_common(0x0040cdf6u, 0x0040d028u);
}

/* 0x99fe0, 251 bytes. Draw the box rows for the table at 0x40cefe (count word at 0x40d02c). */
void draw_box_rows_99fe0(void)
{
    draw_box_rows_common(0x0040cefeu, 0x0040d02au);
}

/* 0x9a0dc, 251 bytes. Draw the box rows for the table at 0x40cd7e (count word at 0x40d026). */
void draw_box_rows_9a0dc(void)
{
    draw_box_rows_common(0x0040cd7eu, 0x0040d024u);
}

/* 0x9a1d8, 251 bytes. Draw the box rows for the table at 0x40ce86 (count word at 0x40d02e). */
void draw_box_rows_9a1d8(void)
{
    draw_box_rows_common(0x0040ce86u, 0x0040d02cu);
}
