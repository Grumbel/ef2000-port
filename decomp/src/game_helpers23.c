/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Marker drawing and unit refresh helpers, 0x2a1c4..0x4d1ec. */

/* 0x2a1c4, 54 bytes. Re-evaluate a record's state byte and refresh the object. */
void refresh_record_2a1c4(ef_addr32 record)
{
    int32_t id = (int16_t)EF_U16(record + 0x16u);

    if ((guest_fn_2949c(id, 0x1e) & 0xffu) != 0x14)
        guest_fn_2a1fc((int16_t)EF_U16(record + 0x16u), 0);
    guest_fn_6b54c(record);
}

/* 0x2af78, 81 bytes. Sum the weights of the six linked units of an owner. */
uint32_t sum_owner_weights_2af78(int16_t owner, uint8_t filter)
{
    uint32_t total = 0;
    int16_t i;

    for (i = 0; i < 6; i++) {
        ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca);
        int16_t id = (int16_t)EF_U16(record + (uint32_t)((int32_t)i * 2) + 0x50u);

        total += sum_slot_weights_755c8(id, filter);
    }
    return total;
}

/* 0x3bc60, 47 bytes. Draw a small cross of arm length 3 at (x, y). */
void draw_cross_3bc60(uint32_t marker, int32_t x, int32_t y)
{
    draw_tactical_line(y, x + 3, y, x - 3, marker);
    draw_tactical_line(y + 3, x, y - 3, x, marker);
}

/* 0x3bc90, 47 bytes. Draw a small box around (x, y); half-size 2 in mode 3, otherwise 1. */
void draw_box_3bc90(int16_t colour, int32_t x, int32_t y)
{
    int32_t half = (EF_U8(0x0012635fu) == 3) ? 2 : 1;

    guest_fn_cec00((int32_t)colour, x - half, y - half, x + half, y + half);
}

/* 0x4d1ec, 94 bytes. Blit the cursor image for the current screen (when the page matches key). */
void blit_cursor_4d1ec(int32_t x, int32_t y, uint32_t key)
{
    if (EF_U8(0x001a53dcu) == 0 || key != EF_U32(0x001a5506u))
        return;
    {
        int32_t row = (int16_t)EF_U16(0x001346cfu);
        int32_t column = (int16_t)EF_U16(0x001346cdu);
        uint32_t source = EF_U32(0x00126570u) + (uint32_t)(column + row * 640);

        guest_fn_ca620((int32_t)source, (int16_t)(x - 0x16), (int16_t)(y - 0x15), 0x2c, 0x2a, 0x280);
    }
}
