/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t frame_coordinate(uint32_t value)
{
    return (uint32_t)(int32_t)(int16_t)value;
}

/* 0xa5ca4,596 bytes. EAX widget, six other GPRs preserved, plain RET.
 * EDI is cleared before use; it is not an implicit argument.
 * Sole direct caller a5fd2 retains its widget pointer in EDX. */
void draw_widget_frame_a5ca4(ef_addr32 widget)
{
    uint32_t x0, y0, x1, y1, index, width_minus_one;

    /* Reload the widget fields between calls: drawing can alias guest RAM.
     * Keep even the original rectangle whose Y endpoints are reversed. */
    y1 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 4u);
    x0 = frame_coordinate(EF_U16(widget + 0x41u) + 3u);
    y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x73u));
    x1 = x0;
    set_clip_rect_c94f4(EF_U8(0x00137947u), x0, y0, x1, y1);

    y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 4u);
    x1 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 2u);
    x0 = frame_coordinate(EF_U16(widget + 0x41u) + 4u);
    y1 = y0;
    set_clip_rect_c94f4(EF_U8(0x00137947u), x0, y0, x1, y1);

    y1 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x73u) - 1u);
    x0 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 2u);
    y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 4u);
    x1 = x0;
    set_clip_rect_c94f4(EF_U8(0x00137947u), x0, y0, x1, y1);

    y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x73u) - 1u);
    x1 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 2u);
    x0 = frame_coordinate(EF_U16(widget + 0x41u) + 3u);
    y1 = y0;
    set_clip_rect_c94f4(EF_U8(0x00137947u), x0, y0, x1, y1);

    index = 0u;
    do {
        y1 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 1u);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + index);
        y0 = frame_coordinate(EF_U16(widget + 0x43u) + 3u);
        x1 = x0;
        set_clip_rect_c94f4(EF_U8(0x00137945u), x0, y0, x1, y1);

        y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 3u + index);
        x1 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 4u);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + 3u);
        y1 = y0;
        ++index;
        set_clip_rect_c94f4(EF_U8(0x00137945u), x0, y0, x1, y1);
    } while (index < 3u);

    if (EF_U16(widget + 0xc5u) != 0u) {
        width_minus_one = EF_U16(widget + 0x45u) - 1u;
        x1 = frame_coordinate(EF_U16(widget + 0x41u) + width_minus_one);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + 3u);
        y0 = (uint32_t)(EF_S32(widget + 0x41u) >> 16);
        y1 = y0;
        set_clip_rect_c94f4(EF_U8(0x0013794au), x0, y0, x1, y1);

        y1 = frame_coordinate(EF_U16(widget + 0x43u) + 18u);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + width_minus_one);
        y0 = (uint32_t)(EF_S32(widget + 0x41u) >> 16);
        x1 = x0;
        set_clip_rect_c94f4(EF_U8(0x0013794au), x0, y0, x1, y1);

        y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x73u) - 2u);
        x1 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 1u);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + width_minus_one);
        y1 = y0;
    } else {
        x1 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 1u);
        x0 = frame_coordinate(EF_U16(widget + 0x41u) + 3u);
        y0 = (uint32_t)(EF_S32(widget + 0x41u) >> 16);
        y1 = y0;
    }
    set_clip_rect_c94f4(EF_U8(0x0013794au), x0, y0, x1, y1);

    y1 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x47u) - 4u);
    x0 = frame_coordinate(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u) - 1u);
    y0 = frame_coordinate(EF_U16(widget + 0x43u) + EF_U16(widget + 0x73u) - 2u);
    x1 = x0;
    set_clip_rect_c94f4(EF_U8(0x0013794au), x0, y0, x1, y1);
}
