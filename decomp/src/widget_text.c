/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa6c20, 203 bytes. EAX widget, six other GPRs preserved, plain RET.
 * Sole caller a7082 retains ESI widget. The original copies unchecked into
 * a 132-byte stack buffer before drawing; retain that behavior. */
void draw_widget_text_a6c20(ef_addr32 widget)
{
    uint8_t text[132];
    int32_t start, clip_start, clip_size;
    uint32_t x, y, style;

    start = (int32_t)EF_U32(widget + 0x2bu) >> 16;
    start += (int32_t)EF_U32(0x0041ec00u) >> 16;
    clip_size = (int32_t)EF_U32(0x0041ec0cu) >> 16;
    clip_start = (int32_t)EF_U32(0x0041ec08u) >> 16;
    if (start >= clip_start + clip_size)
        return;
    start += EF_U16(widget + 0x31u);
    if (start < clip_start)
        return;
    start = (int32_t)EF_U32(widget + 0x29u) >> 16;
    start += (int32_t)EF_U32(0x0041ebfeu) >> 16;
    clip_size = (int32_t)EF_U32(0x0041ec0au) >> 16;
    clip_start = (int32_t)EF_U32(0x0041ec06u) >> 16;
    if (start >= clip_start + clip_size)
        return;
    start += EF_U16(widget + 0x2fu);
    if (start < clip_start)
        return;

    guest_fn_9d749((ef_addr32)(uintptr_t)text, EF_U32(widget + 0x27u));
    x = EF_U16(0x0041ec00u);
    y = EF_U16(0x0041ec02u);
    x += EF_U16(widget + 0x2bu);
    y += EF_U16(widget + 0x2du);
    style = EF_U8(widget + 0x8bu) >> 4;
    draw_styled_text_e26bc(0, (int16_t)x, (int16_t)y,
                  (ef_addr32)(uintptr_t)text, style);
}
