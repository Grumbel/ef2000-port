/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Widget rectangle drawing from the 10-byte record table at 0x127a96. */

/* 0x49144, 156 bytes. Draw the outline of widget record idx: its centre (x,y) and size (w,h) become a rectangle; mode 0 when the record's kind byte is 3, else 3. */
void draw_widget_rect_49144(int16_t idx)
{
    ef_addr32 rec = 0x00127a98u + (uint32_t)(int32_t)idx * 10u;
    int32_t x = EF_S16(rec);
    int32_t y = EF_S16(rec + 2u);
    int32_t half_w = EF_S16(rec + 4u) >> 1;
    int32_t half_h = EF_S16(rec + 6u) >> 1;
    int32_t mode = (EF_U8(rec + 9u) == 3) ? 0 : 3;

    guest_fn_ceccc(mode, x - half_w, y - half_h, x + half_w, y + half_h);
}

/* 0x22e98, 142 bytes. Highlight widget record idx: when its slot byte is set, look the text slot up and enter scroll mode, then draw the rectangle and leave scroll mode. */
void highlight_widget_22e98(int16_t idx)
{
    ef_addr32 rec = 0x00127a98u + (uint32_t)(int32_t)idx * 10u;
    int8_t slot = (int8_t)EF_U8(rec + 8u);
    int32_t x;
    int32_t y;
    int32_t half_w;
    int32_t half_h;

    if (slot != -1) {
        uint32_t r = get_text_slot_cd7f0((int16_t)slot);

        if (r == 0)
            return;
        enter_scroll_mode_c39f0(r);
    }
    x = EF_S16(rec);
    y = EF_S16(rec + 2u);
    half_w = EF_S16(rec + 4u) >> 1;
    half_h = EF_S16(rec + 6u) >> 1;
    guest_fn_ceccc(3, x - half_w, y - half_h, x + half_w, y + half_h);
    leave_scroll_mode_c39ac();
}
