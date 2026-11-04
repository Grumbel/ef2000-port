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

/* 0x103d80, 138 bytes. One-time setup of the runtime's locked ranges: register the code range 0x103d80..0x1046c4 and the six data ranges below, then mark it done at 0x17d2d4. */
void init_locked_ranges_103d80(void)
{
    if (EF_U32(0x0017d2d4u) != 0u)
        return;
    guest_fn_104840(0x00103d80u, 0x001046c4u);
    stack_range_104908(0x004dd670u, 0x40u);
    stack_range_104908(0x004dd7ccu, 0x4cu);
    stack_range_104908(0x004dd6b0u, 0x100u);
    stack_range_104908(0x004dd7b0u, 0x18u);
    stack_range_104908(0x0017d2d0u, 0x4u);
    stack_range_104908(0x004dd7c8u, 0x4u);
    EF_U32(0x0017d2d4u) = 1u;
}

/* 0x78ac0, 139 bytes. While the pending-message count at 0x1f8d56 is positive and 0x127da8 is clear, shrink it to the limit at 0x127d92 and redraw the uppercased message at row 0xc; otherwise clear the count. */
void redraw_pending_message_78ac0(void)
{
    int32_t limit, count, width;
    if (EF_U8(0x00127da8u) != 0u)
        return;
    if (EF_S16(0x001f8d56u) <= 0) {
        EF_U16(0x001f8d56u) = 0u;
        return;
    }
    limit = (int32_t)EF_U16(0x00127d92u);
    count = EF_S32(0x001f8d54u) >> 16;
    if (limit >= count)
        limit = count;
    EF_U16(0x001f8d56u) = (uint16_t)(count - limit);
    uppercase_string(0x001f8d00u);
    EF_U8(0x00127b1au) = 1u;
    width = (int32_t)(int16_t)guest_fn_48d5c_x(0x001f8d00u);
    guest_fn_48c90(0xcu, (uint32_t)(int32_t)(int16_t)(0x136 - width), 0x1eu, 0x001f8d00u);
}

/* 0x35b2c, 139 bytes. Commit the chosen game mode: copy the selection (0x1357ae) to the mode/slot bytes, format its name for mode 3, store the 0x12583b flag into the named setting and set the update flags. */
void commit_mode_selection_35b2c(void)
{
    uint32_t value;
    ef_addr32 setting;
    if (EF_U8(0x00125846u) != 1u)
        EF_U8(0x00125838u) = EF_U8(0x001357aeu);
    if (EF_U8(0x00125846u) == 3u)
        guest_sprintf_a4342(0x00125828u, 0x00183080u, (uint32_t)EF_U16(0x001357aeu));
    EF_U8(0x001a666bu) = EF_U8(0x001357aeu);
    EF_U8(0x00127da9u) = 1u;
    value = EF_U8(0x0012583bu);
    setting = lookup_key_a7ec4(0x0018308cu);
    EF_U32(setting) = value;
    if (EF_U8(0x00125846u) == 1u)
        EF_U8(0x00127da8u) = 1u;
    EF_U8(0x00125847u) = 1u;
    EF_U8(0x001264e2u) = EF_U8(0x0012584au);
}
