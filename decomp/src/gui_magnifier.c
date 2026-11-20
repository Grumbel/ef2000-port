/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Magnifier widget drawing. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xab95c, 404 bytes (tracker name guiUpdateMemoryUse), draw callback
 * of a magnifier widget. The source area is the widget's size (+0x75,
 * +0x77) divided by the zoom factor, centred on the pointer and kept
 * inside the screen. Each source pixel of the framebuffer is drawn as a
 * ZOOM x ZOOM block into the active widget area, for as many whole
 * blocks as fit. */
void guiUpdateMemoryUse(ef_addr32 widget)
{
    int32_t zoom = (int32_t)g_magnifier_zoom;
    int32_t view_w = (int32_t)(int16_t)EF_U16(widget + 0x75u) / zoom;
    int32_t view_h = (int32_t)(int16_t)EF_U16(widget + 0x77u) / zoom;
    int16_t src_x = (int16_t)(g_pointer_x - (int16_t)EF_U16(widget + 0x75u) / (2 * zoom));
    int16_t src_y = (int16_t)(g_pointer_y - (int16_t)EF_U16(widget + 0x77u) / (2 * zoom));
    ef_addr32 src_row;
    uint16_t dest_y = (uint16_t)g_active_area_y;
    int16_t row;

    if (src_x < 0)
        src_x = 0;
    if (src_y < 0)
        src_y = 0;
    if ((int32_t)src_x + view_w > (int32_t)g_video_width)
        src_x = (int16_t)((int32_t)g_video_width - view_w);
    if ((int32_t)src_y + view_h > (int32_t)g_video_height)
        src_y = (int16_t)((int32_t)g_video_height - view_h);
    src_row = g_raster_framebuffer + (uint32_t)((int32_t)src_y * (int32_t)g_video_width) + (uint32_t)(int32_t)src_x;

    for (row = 0; ((int32_t)row + 1) * zoom < (int32_t)g_active_area_height; ++row) {
        ef_addr32 src = src_row;
        uint16_t dest_x = (uint16_t)g_active_area_x;
        int16_t column;

        for (column = 0; ((int32_t)column + 1) * zoom < (int32_t)g_active_area_width; ++column) {
            fill_rect_ceccc(EF_U8(src), dest_x, dest_y, (int32_t)dest_x + zoom, (int32_t)dest_y + zoom);
            ++src;
            dest_x = (uint16_t)(dest_x + zoom);
        }
        dest_y = (uint16_t)(dest_y + zoom);
        src_row += g_video_width;
    }
}
