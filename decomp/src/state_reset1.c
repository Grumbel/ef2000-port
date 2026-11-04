/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* State resets. */

/* 0xa29f8, 126 bytes. Reset the input/menu state: default words and the 7-byte block at 0x13579b, clear the table, then set the control flags. */
void reset_input_state_a29f8(void)
{
    uint32_t i;
    EF_U8(0x001357c8u) = 0u;
    EF_U8(0x0013579au) = 0u;
    EF_U16(0x001357b4u) = 8u;
    EF_U16(0x001357b8u) = 0xffffu;
    for (i = 0; i < 7u; i++)
        EF_U8(0x0013579bu + i) = EF_U8(0x0018cc24u + i);
    EF_U8(0x001357b6u) = 1u;
    reset_table_ece90();
    EF_U8(0x0015f07cu) = 0u;
    EF_U8(0x0015f07du) = 0u;
    EF_U8(0x0015f07eu) = 0u;
    EF_U8(0x0015f07fu) = 1u;
    EF_U8(0x0015f099u) = 1u;
    EF_U8(0x0015f098u) = 0u;
    EF_U8(0x0015f688u) = 0u;
    EF_U8(0x0015f09au) = 3u;
}

/* 0x2ce18, 127 bytes. Reset a 0xca-byte page record: clear the object limit of each of its six slots (unless 0xff), recompute its grid limit, and copy the two saved dwords at +0x99/+0x9d over +0x91/+0x95. */
void reset_page_record_2ce18(uint32_t page)
{
    int16_t p = (int16_t)page;
    uint32_t rec;
    int16_t i;
    if ((int32_t)p == 0xff)
        return;
    for (i = 0; i < 6; i++) {
        uint32_t base = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)p * 0xca) + (uint32_t)i * 2u;
        if (EF_U16(base + 0x50u) != 0xffu)
            reset_object_limit((int16_t)EF_S16(base + 0x50u));
    }
    recompute_grid_limit(p);
    rec = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)p * 0xca);
    EF_U32(rec + 0x91u) = EF_U32(rec + 0x99u);
    EF_U32(rec + 0x95u) = EF_U32(rec + 0x9du);
}
