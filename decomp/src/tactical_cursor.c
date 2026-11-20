/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* The cosine table is the sine table read a quarter turn later. */
#define COSINE_TABLE 0x00142e08u

static int32_t position_word(ef_addr32 target, uint32_t offset)
{
    return (int32_t)(int16_t)(EF_S32(target + offset) >> 8);
}

/* Move the cursor one step of the tick speed toward SCREEN, snapping
   each axis that is closer than the step. Once both axes are reached
   the cursor locks onto the target. */
static void step_cursor_toward(const int16_t *screen)
{
    int32_t dx = (int16_t)(screen[0] - g_tactical_cursor_x);
    int32_t dy = (int16_t)(screen[1] - g_tactical_cursor_y);
    uint32_t angle = ((uint32_t)angle_of_b3b1c(dx, dy) & 0xffffu) >> 3;
    uint32_t speed = (uint32_t)g_simulation_tick_step << g_tactical_display_mode;
    int16_t step_x = (int16_t)((int32_t)((uint32_t)(int32_t)g_sine_table[angle] * speed) >> 15);
    int16_t step_y = (int16_t)((int32_t)(speed * (uint32_t)(int32_t)EF_S16(COSINE_TABLE + 2u * angle)) >> 15);

    if ((int32_t)abs_a28f2(step_x) > (int32_t)abs_a28f2(dx))
        g_tactical_cursor_x = screen[0];
    else
        g_tactical_cursor_x = (int16_t)(g_tactical_cursor_x + step_x);
    if ((int32_t)abs_a28f2(step_y) > (int32_t)abs_a28f2(dy))
        g_tactical_cursor_y = screen[1];
    else
        g_tactical_cursor_y = (int16_t)(g_tactical_cursor_y + step_y);

    if (screen[0] == g_tactical_cursor_x && screen[1] == g_tactical_cursor_y)
        g_tactical_tracked = g_tactical_target;
}

/* 0x59fa4, 633 bytes. Tactical display cursor for the watched unit:
   latch the display bounds, project the target onto the display, move
   the cursor toward it (or onto it once tracked) and mark it; a target
   off the display gets a bearing marker at the edge and the cursor
   returns to the display centre. The range marker is drawn last. */
void update_tactical_cursor_59fa4(void)
{
    int16_t screen[2];
    ef_addr32 target;
    uint8_t on_screen;
    uint8_t off_display = 0;

    if (lookup_filtered_indexed_object(g_watched_unit) == 0)
        return;

    latch_values_ce328((uint32_t)(int32_t)g_tactical_min_x, (uint32_t)(int32_t)g_tactical_max_x,
                       (uint32_t)(int32_t)g_tactical_min_y, (uint32_t)(int32_t)g_tactical_max_y);
    store_four_dwords_17cf9c((uint32_t)(int32_t)g_tactical_min_x, (uint32_t)(int32_t)g_tactical_max_x,
                             (uint32_t)(int32_t)g_tactical_min_y, (uint32_t)(int32_t)g_tactical_max_y);

    target = g_tactical_target;
    if (target == 0) {
        g_tactical_tracked = 0;
        draw_range_marker_5df08();
        return;
    }

    on_screen = (uint8_t)guest_fn_15c0c4_x((uint32_t)position_word(target, 8u), (uint32_t)position_word(target, 4u),
                                           (uint32_t)position_word(target, 0u), (ef_addr32)(uintptr_t)screen);
    if (on_screen) {
        if (!position_within_tactical_bounds(screen[0], screen[1])) {
            off_display = 1;
            on_screen = 0;
        } else if (g_tactical_tracked == g_tactical_target) {
            g_tactical_cursor_x = screen[0];
            g_tactical_cursor_y = screen[1];
        } else {
            step_cursor_toward(screen);
        }
        draw_object_marker_5d520(screen[0], screen[1], 0);
    }

    if (g_tactical_tracked != 0 && !within_unit_radius_66b74(g_tactical_tracked)) {
        g_tactical_tracked = 0;
        on_screen = 0;
    }
    if (on_screen) {
        draw_tactical_cross(g_tactical_cursor_x, g_tactical_cursor_y);
    } else {
        g_tactical_tracked = 0;
        g_tactical_cursor_x = g_tactical_coordinate_offset_x;
        g_tactical_cursor_y = g_tactical_coordinate_offset_y;
        if (off_display)
            draw_bearing_marker_5d080((ef_addr32)(uintptr_t)screen);
    }
    draw_range_marker_5df08();
}
