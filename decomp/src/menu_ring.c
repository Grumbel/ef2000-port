/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

#define MENU_RING_MAX_POINTS 0x46
#define MENU_RING_SWEEP 0xc000 /* three quarters of a turn */

/* Q15 product, truncated toward zero. */
static int32_t q15_mul(int32_t a, int32_t b)
{
    return a * b / 0x8000;
}

static void set_ring_point(int16_t *points, int16_t index, int16_t x, int16_t y, int32_t sine, int32_t cosine,
                           int32_t radius)
{
    points[index * 2] = (int16_t)(q15_mul(sine, radius) + x);
    points[index * 2 + 1] = (int16_t)(y - q15_mul(cosine, radius));
}

/* 0x5e4c4, 403 bytes (2 stack arguments, ret 8). Lay out COUNT menu
 * points on a three-quarter ring around (X, Y): the points at RADIUS
 * and at RADIUS -/+ WIDTH go to g_menu_ring_points, g_menu_ring_inner
 * and g_menu_ring_outer. COUNT above 0x46 is ignored. Sets
 * g_unit_counter to 1, both ring counts to COUNT, the cursor to 0 and
 * the style word to STYLE. */
void layout_menu_ring_5e4c4(int16_t x, int16_t y, int16_t radius, int16_t count, uint32_t style, int16_t width)
{
    int16_t i;

    if (count > MENU_RING_MAX_POINTS)
        return;
    g_unit_counter = 1;
    for (i = 0; i < count; ++i) {
        uint16_t angle = (uint16_t)((int32_t)i * MENU_RING_SWEEP / (int32_t)count);
        int32_t index = angle >> 3;
        int32_t sine = g_sine_table[index];
        int32_t cosine = g_sine_table[index + 0x800];

        set_ring_point(g_menu_ring_points, i, x, y, sine, cosine, radius);
        set_ring_point(g_menu_ring_outer, i, x, y, sine, cosine, (int32_t)radius + width);
        set_ring_point(g_menu_ring_inner, i, x, y, sine, cosine, (int32_t)radius - width);
    }
    g_menu_ring_count = (uint16_t)count;
    g_menu_ring_shown = (uint16_t)count;
    g_menu_ring_cursor = 0;
    g_menu_ring_style = (uint16_t)style;
}
