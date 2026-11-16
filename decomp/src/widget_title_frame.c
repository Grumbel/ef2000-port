/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa713c, 199 bytes. EAX widget, all six other GPRs preserved.
 * Sole direct caller a70be retains ESI widget. The checked lookup finds
 * the title control, whose selected state chooses the source bitmap. */
void draw_widget_frame_a713c(ef_addr32 widget)
{
    int32_t x, width, y, height, stride;
    int32_t left, extent;
    uint32_t bitmap;
    ef_addr32 title;

    if ((EF_U8(widget + 0x3au) & 0x40u) == 0u)
        return;
    if ((EF_U8(widget + 0x3au) & 1u) == 0u)
        return;
    left = (int32_t)EF_U32(widget + 0x3fu) >> 16;
    extent = (int32_t)EF_U32(widget + 0x43u) >> 16;
    if (left + extent > (int32_t)EF_U32(0x0015b744u))
        return;

    title = find_widget_checked_a78bc(widget, 0x0018d1ecu);
    x = EF_U16(widget + 0x41u);
    x += EF_U16(title + 4u);
    width = EF_U16(widget + 0x45u);
    width += EF_U16(widget + 0x41u);
    width -= x;
    if ((EF_U8(title + 0x1du) & 1u) != 0u) {
        bitmap = EF_U32(0x0041ec1cu);
    } else {
        bitmap = EF_U32(0x0041ec24u);
        width--;
    }
    y = (int16_t)(EF_U16(widget + 0x43u) + 1u);
    height = (int32_t)EF_U32(title + 4u) >> 16;
    x = (int16_t)x;
    stride = (int32_t)EF_U32(0x0015b742u) >> 16;
    width = (int16_t)width;
    draw_clipped_c92e4((uint32_t)x, (uint32_t)y, (uint32_t)width,
                      (uint32_t)height, (uint32_t)stride,
                      (uint32_t)x, (uint32_t)y, bitmap);
}
