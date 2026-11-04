/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Player state checks, statistics counters and view setup (190..230 bytes). */

/* 0x1abbc, 195 bytes. Run the fire action when the selected weapon kind allows it. */
void try_fire_selected_weapon_1abbc(void)
{
    ef_addr32 unit = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    ef_addr32 player;
    int32_t slot;
    int32_t kindIndex;
    uint8_t kind;

    if (unit == 0 || (EF_U8(unit + 0x24u) & 4) || (EF_U8(unit + 0x24u) & 8) || EF_U8(0x00132ac2u) != 0
        || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0 || EF_U16(0x00126516u) != 0
        || EF_U8(0x00122c5du) != 0)
        return;
    player = EF_U32(0x001f71c4u);
    slot = EF_S32(player + 0x1d4u) >> 24;
    kindIndex = EF_S32(player + (uint32_t)(slot * 3) + 0x198u) >> 24;
    kind = EF_U8((uint32_t)(kindIndex * 145) + 0x0012bcacu);
    if (kind == 1 || (kind == 2 && (EF_U8(0x001a3646u) == 3 || EF_U8(0x001a3646u) == 4)))
        guest_fn_17754_w();
}

/* 0x8cb60, 222 bytes. Refresh the player's weapon state and the activity display. */
void refresh_weapon_state_8cb60(void)
{
    ef_addr32 unit = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    int32_t slot;
    int32_t kindIndex;

    if (unit == 0)
        return;
    if (EF_U8(0x001f6f4fu) != 0)
        guest_fn_8cae8_w();
    else
        guest_fn_8ca24_w(EF_S32(unit + 0x26u) >> 16);
    if (EF_U8(0x00127da8u) != 0)
        EF_U8(0x001f6f4fu) = 0;
    EF_U8(unit + 0x1d7u) = first_nonignored_activity_slot((int16_t)(EF_S32(unit + 0x14u) >> 16));
    slot = EF_S32(unit + 0x1d4u) >> 24;
    kindIndex = EF_S32(unit + (uint32_t)(slot * 3) + 0x198u) >> 24;
    EF_U8(0x0012814cu) = (uint8_t)((EF_U8((uint32_t)(kindIndex * 145) + 0x0012bc94u) != 1) + 3);
    reset_object_limit((int16_t)(EF_S32(unit + 0x14u) >> 16));
    EF_U32(0x001a30a0u) = EF_U32(unit + 0x177u);
    EF_U32(0x001a30a4u) = EF_U32(unit + 0x17bu);
    EF_U8(0x001a3631u) = 0;
    EF_U8(0x001a363du) = 0;
    EF_U32(0x001a3110u) = 0;
    EF_U32(0x001a3114u) = 0;
    guest_fn_86720_w();
}

/* 0x94000, 222 bytes. Compute the scene angles from the stored vector and flag out-of-range input. */
void init_scene_angles_94000(void)
{
    uint32_t a;
    uint32_t b;
    int32_t x;
    int32_t y;

    EF_U16(0x0012dba0u) = 0;
    EF_U16(0x0012dba6u) = 0;
    guest_fn_d9ba0_w(EF_U32(0x00214358u), 3, 0x00214360u, 0x0021435cu, 0x00214364u, 0x00214368u);
    init_bounds_d0b68(0x0012dbecu);
    a = (uint16_t)(-(int32_t)EF_U16(0x00214360u));
    guest_fn_d0ebc(0x0012dbecu, a);
    b = (uint16_t)(-(int32_t)EF_U16(0x0021435cu));
    guest_fn_d0dbc(0x0012dbecu, b, 0x00214360u);
    guest_fn_d0fb8(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x00214364u)), 0x0021435cu);
    x = (int32_t)EF_U32(0x0021435cu);
    y = (int32_t)EF_U32(0x00214360u);
    EF_U8(0x001a6d1fu) = ((x >= 0x258 && x <= 0xfde8) || (y >= 0x258 && y <= 0xfde8)) ? 1 : 0;
    EF_U16(0x0012dc80u) = 0x47;
    guest_fn_93a8c();
    guest_fn_95018();
}

/* 0x9a8b4, 228 bytes. Count a destroyed ground object in the statistics table by its class. */
void count_ground_kill_9a8b4(ef_addr32 record, uint32_t kind)
{
    int32_t v = EF_S32(record + 0x18u) >> 16;
    uint32_t base;

    if (v != -1)
        kind = EF_U8(guest_fn_c1b8c_w(v, 0x00137970u) + 0xcu);
    kind &= 0xff;
    if (kind == 0xff)
        return;
    base = (uint32_t)(EF_S32(0x0040d026u) >> 16) * 2u;
    if ((uint16_t)guest_fn_cc584_w(kind) != 0)
        EF_U16(base + 0x0040cfb4u)++;
    else if ((uint16_t)guest_fn_cc5fc_w(kind) != 0)
        EF_U16(base + 0x0040cfbau)++;
    else if ((uint16_t)guest_fn_cc628_w(kind) != 0)
        EF_U16(base + 0x0040cfc0u)++;
    else if ((uint16_t)guest_fn_cc654_w(kind) != 0)
        EF_U16(base + 0x0040cfc6u)++;
    else if ((uint16_t)guest_fn_cc67c_w(kind) != 0)
        EF_U16(base + 0x0040cfccu)++;
    else if ((uint16_t)guest_fn_cc694_w(kind) != 0 || (uint16_t)guest_fn_cc6ac_w(kind) != 0)
        EF_U16(base + 0x0040cfd2u)++;
}

/* 0x9a998, 224 bytes. Count a destroyed air object in the statistics table by its class. */
void count_air_kill_9a998(ef_addr32 record)
{
    uint32_t kind = EF_U8(guest_fn_c1b8c_w(EF_S32(record + 0x18u) >> 16, 0x00137970u) + 0xcu);
    uint32_t base = (uint32_t)(EF_S32(0x0040d026u) >> 16) * 2u;

    if ((EF_S32(record + 0xeu) >> 16) == 2) {
        if (guest_fn_41e24_w(kind) != 0)
            EF_U16(base + 0x0040cf90u)++;
        else if (guest_fn_41f00_w(kind) != 0)
            EF_U16(base + 0x0040cf9cu)++;
        else if (guest_fn_41ea0_w(kind) != 0)
            EF_U16(base + 0x0040cf96u)++;
    } else {
        if (guest_fn_41df4_w(kind) != 0)
            EF_U16(base + 0x0040cfa2u)++;
        else if (guest_fn_41ec8_w(kind) != 0)
            EF_U16(base + 0x0040cfaeu)++;
        else if (guest_fn_41e84_w(kind) != 0)
            EF_U16(base + 0x0040cfa8u)++;
    }
}
