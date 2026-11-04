/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Clipped screen operations and buffer setup. */

/* 0xc3770, 121 bytes. Allocate the two work buffers at 0x146e40/0x146e44 (size 0x164600) if needed, complain when either failed, then zero both. */
void init_work_buffers_c3770(void)
{
    if (EF_U32(0x00146e40u) == 0u)
        EF_U32(0x00146e40u) = guest_fn_c3f70(EF_U32(0x00164600u));
    if (EF_U32(0x00146e44u) == 0u)
        EF_U32(0x00146e44u) = guest_fn_c3f70(EF_U32(0x00164600u));
    if (EF_U32(0x00146e40u) == 0u || EF_U32(0x00146e44u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018ee8cu);
    fill_bytes_b3fb0(EF_U32(0x00146e40u), 0u, EF_U32(0x00164600u));
    fill_bytes_b3fb0(EF_U32(0x00146e44u), 0u, EF_U32(0x00164600u));
}

/* 0x8bd58, 121 bytes. Reset the UI state (record table, four work areas, pool) and then run 0x8bad4 with the caller's two arguments. */
uint32_t reset_ui_state_8bd58(uint32_t a, uint32_t b)
{
    EF_U32(0x0020f420u) = EF_U32(0x00132ad4u);
    load_record_table_8fe58();
    fill_bytes_b3fb0(0x002117b0u, 0u, 0x238u);
    fill_bytes_b3fb0(0x0020f240u, 0u, 0xa0u);
    fill_bytes_b3fb0(0x0020f380u, 0u, 0xa0u);
    fill_bytes_b3fb0(0x0020f2e0u, 0u, 0xa0u);
    reset_ui_record_pool();
    clear_dword_12db5c();
    guest_fn_8b7e8_x();
    return guest_fn_8bad4_x(a, b);
}

/* 0xc92e4, 122 bytes. Forward a rectangle to 0xeb174 when it lies wholly inside the screen (limits 0x15b744/0x15b748). */
void draw_clipped_c92e4(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    int32_t sx = (int16_t)x, sy = (int16_t)y;
    if (sx >= (int32_t)EF_U32(0x0015b744u))
        return;
    if (sy >= (int32_t)EF_U32(0x0015b748u))
        return;
    if (sx + (int32_t)(int16_t)w > (int32_t)EF_U32(0x0015b744u))
        return;
    if (sy + (int32_t)(int16_t)h > (int32_t)EF_U32(0x0015b748u))
        return;
    guest_fn_eb174_x(0u, 0u, 0u, 0u, x & 0xffffu, y & 0xffffu, w & 0xffffu, h & 0xffffu,
                     s0 & 0xffffu, s1 & 0xffffu, s2 & 0xffffu, s3);
}

/* 0xc8e8, 122 bytes. For the current page (word at 0x121e98) call 0x11560 once for each of its entries, passing per-entry slots of the tables at 0x197300..0x197360. */
void draw_page_entries_c8e8(void)
{
    int16_t i = 0;
    for (;;) {
        uint32_t page = (uint32_t)(int32_t)EF_S16(0x00121e98u);
        uint32_t off = (page & 0xffffffffu) * 2u;
        int32_t slot2;
        int16_t colour;
        if (!(i < EF_S16(0x001a53b0u + off)))
            break;
        slot2 = (int32_t)i * 2;
        colour = (int16_t)EF_U16(0x001a52b0u + page * 16u + (uint32_t)slot2);
        i++;
        guest_fn_11560_x((int32_t)EF_S16(0x00197696u),
                         (int32_t)EF_S16(0x001a5280u + off),
                         (int32_t)colour,
                         0x00197330u + (uint32_t)slot2,
                         0x00197300u + (uint32_t)slot2,
                         0x00197340u + (uint32_t)slot2,
                         0x00197360u + (uint32_t)slot2,
                         0x00197350u + (uint32_t)slot2);
    }
}

/* 0xdd8dc, 170 bytes. Save the screen area under the parameter block at 0x15ee80 (size clamped against 0x425fc6/0x425fc8) into its buffer and draw the block via 0xcde00. */
void save_and_draw_block_dd8dc(void)
{
    int16_t w = (int16_t)(EF_U16(0x00425fc8u) - EF_U16(0x0015ee8cu));
    int16_t h = (int16_t)(EF_U16(0x00425fc6u) - EF_U16(0x0015ee8eu));
    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;
    get_screen_rows_eb1f0(0u, 0u, 0u, 0u, (uint16_t)w, (uint16_t)h, EF_U16(0x0015ee88u), EF_U16(0x0015ee8au), EF_U32(0x0015ee90u));
    EF_U16(0x0015ee94u) = (uint16_t)w;
    EF_U16(0x0015ee96u) = (uint16_t)h;
    guest_fn_cde00_x((uint32_t)((int32_t)EF_S16(0x0015ee86u) + (int32_t)EF_S16(0x00425fc6u)),
                     (uint32_t)((int32_t)EF_S16(0x0015ee84u) + (int32_t)EF_S16(0x00425fc8u)),
                     0u, EF_U32(0x0015ee80u));
}

/* 0xa0aa0, 186 bytes. Fetch the 6-byte point entry of the current page (table at 0x41717c + 0xa), let the hook at 0x132af4 transform it, add the offset at 0x417194..98 and store the squared distance from the origin at 0x159094..9c in 0x132abc. */
void measure_page_point_a0aa0(uint32_t index)
{
    ef_addr32 ent = EF_U32(0x0041717cu) + 0xau + (uint32_t)((int32_t)(int16_t)index * 6);
    int16_t x, y, z;
    EF_U16(0x00417226u) = EF_U16(ent + 2u);
    guest_callptr2r_x(EF_U32(0x00132af4u), (int32_t)EF_S16(ent), (int32_t)EF_S16(ent + 4u));
    x = (int16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (int16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (int16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    EF_U16(0x00417224u) = (uint16_t)x;
    EF_U16(0x00417226u) = (uint16_t)y;
    EF_U16(0x00417228u) = (uint16_t)z;
    EF_U16(0x00132abcu) = (uint16_t)length_squared_dec90((int16_t)(x - (int16_t)EF_U16(0x00159094u)),
                                                          (int16_t)(y - (int16_t)EF_U16(0x00159098u)),
                                                          (int16_t)(z - (int16_t)EF_U16(0x0015909cu)));
}

/* 0x9bca4, 83 bytes. Draw a bar of height h (when non-zero) at x: a coloured strip via 0xc99ac and a highlight via 0xc9a98. */
void draw_bar_9bca4(uint32_t colour, uint32_t unused, uint32_t x, uint32_t h)
{
    int32_t sx = (int16_t)x;
    (void)unused;
    if ((uint16_t)h == 0u)
        return;
    guest_fn_c99ac_x(colour & 0xffu, 0x78, sx, (int32_t)(int16_t)(h + 0x78u), (int32_t)(int16_t)(x + 0xeu));
    guest_fn_c9a98_x(8u, 0x78, sx, (int32_t)(int16_t)(h + 0x79u), (int32_t)(int16_t)(x + 0xfu));
}
