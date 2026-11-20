/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define ROUTE_HIDE_LINES      0x02u    /* flags: draw only the triangle markers */
#define ROUTE_PLAIN_MARKERS   0x01u    /* flags: every marker is a box */
#define ROUTE_LINE_COLOUR     0x0f
#define ROUTE_LINE_COLOUR_DIM 0x07
#define ROUTE_BOX_COLOUR      6u
#define ROUTE_TAG_TRIANGLE    5u
#define ROUTE_LABELS_SHOWN    2u       /* g_spatial_label_mode */

#define NUMBER_FORMAT         0x00181e98u /* "%hd" */

/* Filled triangle pointing up, drawn as index stream mode 9 with three points. */
static void draw_triangle_marker(int16_t x, int16_t y)
{
    int16_t stream[8];

    stream[0] = 9;
    stream[1] = 3;
    stream[2] = x;
    stream[3] = (int16_t)(y - 8);
    stream[4] = (int16_t)(x + 7);
    stream[5] = (int16_t)(y + 4);
    stream[6] = (int16_t)(x - 7);
    stream[7] = (int16_t)(y + 4);
    render_index_stream_c4d43((ef_addr32)(uintptr_t)stream);
}

/* 0x26424, 586 bytes. AX dim, DL flags; EAX and EDX clobbered. Draw the
 * route held in g_spatial_slots: unless flags bit 1 is set, a line
 * between each pair of consecutive points (colour 7 when DIM is nonzero,
 * else 15). Then a marker at every point: a triangle for tag 5, otherwise
 * (unless flags bit 1) a colour-6 box as wide as the point's number. Flags
 * bit 0 treats every tag as 1. With g_spatial_label_mode 2 (and flags bit
 * 1 clear) the box is labelled with the running number, starting at 1, in
 * colour 15. */
void draw_spatial_route_26424(uint32_t dim, uint32_t flags)
{
    uint8_t mode = (uint8_t)flags;
    int16_t number = 1;
    uint16_t left;
    int16_t k;

    if (g_spatial_slot_count == 0)
        return;
    if (!(mode & ROUTE_HIDE_LINES)) {
        int32_t colour = (uint16_t)dim != 0 ? ROUTE_LINE_COLOUR_DIM : ROUTE_LINE_COLOUR;

        for (k = 0, left = (uint16_t)(g_spatial_slot_count - 1); left != 0; --left, ++k)
            draw_tactical_line(g_spatial_slots[k + 1].y, g_spatial_slots[k + 1].x,
                               g_spatial_slots[k].y, g_spatial_slots[k].x, (uint32_t)colour);
    }
    if (g_spatial_slot_count == 0)
        return;
    for (k = 0, left = (uint16_t)g_spatial_slot_count; left != 0; --left, ++k) {
        int16_t x = g_spatial_slots[k].x;
        int16_t y = g_spatial_slots[k].y;
        uint16_t tag = g_spatial_slot_tags[k];
        int16_t width = (int16_t)formatted_text_width_48c34(1u, NUMBER_FORMAT, (int32_t)number);
        int16_t half = (int16_t)((int16_t)(width + width % 2) >> 1);

        if (mode & ROUTE_PLAIN_MARKERS)
            tag = 1;
        if (tag == ROUTE_TAG_TRIANGLE)
            draw_triangle_marker(x, y);
        else if (!(mode & ROUTE_HIDE_LINES))
            fill_rect_clipped_cec00(ROUTE_BOX_COLOUR, x - 1 - half, y - 3, x + 1 + half, y + 3);
        if (g_spatial_label_mode == ROUTE_LABELS_SHOWN && !(mode & ROUTE_HIDE_LINES)) {
            draw_formatted_text_mode_48bb0(ROUTE_LINE_COLOUR, 1u, (int16_t)(x - half),
                                           (int16_t)(y - 2), NUMBER_FORMAT, (int32_t)number);
            ++number;
        }
    }
}
