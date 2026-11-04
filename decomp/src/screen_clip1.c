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
