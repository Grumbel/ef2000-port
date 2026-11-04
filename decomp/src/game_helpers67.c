/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit state snapshot and cursor mode selection. */

/* 0x7222c, 354 bytes. Copy the player's tracked state block into a unit record and refresh it. */
void snapshot_unit_state_7222c(int16_t unit)
{
    static const uint32_t map[30][2] = {
        {0x49, 0x1a3020}, {0x4d, 0x1a3024}, {0x51, 0x1a3028}, {0x55, 0x1a302c}, {0x59, 0x1a3030}, {0x5d, 0x1a3034},
        {0x69, 0x1a2fb0}, {0x6d, 0x1a2fb4}, {0x61, 0x1a2fa0}, {0x65, 0x1a2fa4}, {0x71, 0x1a2fd8}, {0x75, 0x1a2fdc},
        {0xa1, 0x1a2f80}, {0xa5, 0x1a2f84}, {0xa9, 0x1a2f78}, {0xad, 0x1a2f7c}, {0xb1, 0x1a2f88}, {0xb5, 0x1a2f8c},
        {0xb9, 0x1a2f90}, {0xbd, 0x1a2f94}, {0xc1, 0x1a2ec8}, {0xc5, 0x1a2ecc}, {0xc9, 0x1a2ed0}, {0xcd, 0x1a2ed4},
        {0xd1, 0x1a2f50}, {0xd5, 0x1a2f54}, {0xd9, 0x1a2f48}, {0xdd, 0x1a2f4c}, {0xe1, 0x1a2f70}, {0xe5, 0x1a2f74}
    };
    ef_addr32 obj = guest_fn_7489c(unit);
    uint32_t i;

    if (obj == 0)
        return;
    for (i = 0; i < 30; i++)
        EF_U32(obj + map[i][0]) = EF_U32(map[i][1]);
    guest_fn_73d50_w(unit);
}

/* 0x6697c, 330 bytes. Pick the cursor mode from the player's selected station kind and the panel mode. */
void select_cursor_mode_6697c(void)
{
    ef_addr32 obj = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    int32_t slot;
    int16_t kind;

    EF_U8(0x001f9098u) = 0;
    if (obj == 0)
        return;
    slot = EF_S32(obj + 0x1d4u) >> 24;
    EF_U8(0x001a81e2u) = 0;
    kind = (int16_t)(int8_t)EF_U8(obj + (uint32_t)slot * 3u + 0x19bu);
    switch (EF_U8(0x0012814cu)) {
    case 3:
        if (kind == 1) {
            EF_U8(0x001282c5u) = 6;
        } else if (kind == 2) {
            if (EF_U32(0x001a8210u) == 0)
                EF_U8(0x001f9098u) = 1;
            else
                EF_U8(0x001f9098u) = 2;
            EF_U8(0x001282c5u) = 0;
        } else if (kind == 4 || kind == 5) {
            EF_U8(0x001282c5u) = 1;
        } else {
            EF_U8(0x001282c5u) = 0;
        }
        return;
    case 4:
        if ((uint16_t)kind < 8) {
            if ((uint16_t)kind == 6)
                EF_U8(0x001282c5u) = 7;
            else if ((uint16_t)kind < 6)
                EF_U8(0x001282c5u) = ((uint16_t)kind == 1) ? 6 : 3;
            else
                EF_U8(0x001282c5u) = 4;
        } else if ((uint16_t)kind == 8) {
            EF_U8(0x001282c5u) = 2;
            EF_U8(0x001a81e2u) = 1;
        } else if ((uint16_t)kind < 10) {
            EF_U8(0x001282c5u) = 9;
        } else if ((uint16_t)kind == 0xa || (uint16_t)kind == 0xb || (uint16_t)kind == 0x1c) {
            EF_U8(0x001282c5u) = 5;
        } else {
            EF_U8(0x001282c5u) = 3;
        }
        return;
    case 6:
        EF_U8(0x001282c5u) = 8;
        return;
    default:
        EF_U8(0x001282c5u) = 10;
        return;
    }
}
