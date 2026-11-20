/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define FADE_COLOUR      0x00425c8cu /* palette entry 1 of the menu picture palette */
#define FADE_STEPS       15
#define PALETTE_MAX      0x3fu

static void set_fade_colour(uint8_t level)
{
    EF_U8(FADE_COLOUR + 3u) = level;
    EF_U8(FADE_COLOUR + 4u) = level;
    EF_U8(FADE_COLOUR + 5u) = level;
    apply_palette_d663c(FADE_COLOUR);
}

/* 0x1e480, 499 bytes. AL mode, EDX colour. Play the "picture tube off"
 * effect around the view origin: the full-width band collapses
 * vertically to a line, the line horizontally to a dot, then the dot
 * fades over fifteen frames. Each collapse shrinks by a tenth of the
 * clip extent per frame. Mode 0 draws in colour 1 and first sets it to
 * white, fading it to black (with 0x3f * step / 15) at the end; otherwise
 * the given colour is drawn and stepped down by one per frame. EDX is
 * clobbered. Caller 0x3ef00. */
void collapse_screen_1e480(uint8_t mode, int32_t colour)
{
    int16_t size;
    int32_t step;
    int32_t half;
    int16_t i;

    if (mode == 0u)
        colour = 1;
    latch_values_ce328(0u, g_video_width - 1u, 0u, g_video_height - 1u);
    set_view_origin_dc21c(g_video_height - 1u, g_video_width - 1u,
                          (uint32_t)((int32_t)g_video_height / 2), (uint32_t)((int32_t)g_video_width / 2));
    if (mode == 0u)
        set_fade_colour((uint8_t)PALETTE_MAX);

    size = (int16_t)((uint16_t)g_view_clip_bottom - (uint16_t)g_view_clip_top);
    while (size > 0) {
        cycle_page_dc280();
        step = size;
        clear_buffer_dc2c0();
        half = step / 2;
        fill_rect_ceccc((uint32_t)(int32_t)(int16_t)colour, (int32_t)g_view_clip_left,
                        (int32_t)g_view_origin_y - half, (int32_t)g_view_clip_right,
                        (int32_t)g_view_origin_y + half);
        wait_vsync_dc2e0();
        size = (int16_t)(step - (int32_t)(g_view_clip_bottom - g_view_clip_top) / 10);
    }

    size = (int16_t)((uint16_t)g_view_clip_right - (uint16_t)g_view_clip_left);
    while (size > 0) {
        cycle_page_dc280();
        clear_buffer_dc2c0();
        step = size;
        half = step / 2;
        fill_rect_ceccc((uint32_t)(int32_t)(int16_t)colour, (int32_t)g_view_origin_x - half,
                        (int32_t)g_view_origin_y, (int32_t)g_view_origin_x + half,
                        (int32_t)g_view_origin_y + 1);
        wait_vsync_dc2e0();
        size = (int16_t)(step - (int32_t)(g_view_clip_right - g_view_clip_left) / 10);
    }

    for (i = 0; i < FADE_STEPS; ++i) {
        cycle_page_dc280();
        if (mode == 0u)
            set_fade_colour((uint8_t)((int32_t)i * (int32_t)PALETTE_MAX / FADE_STEPS));
        else
            --colour;
        clear_buffer_dc2c0();
        fill_rect_ceccc((uint32_t)(int32_t)(int16_t)colour, (int32_t)g_view_origin_x,
                        (int32_t)g_view_origin_y, (int32_t)g_view_origin_x + 1,
                        (int32_t)g_view_origin_y + 1);
        wait_vsync_dc2e0();
    }
}
