/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* History shift, state init and owner messages, 0x46070..0x5f588. */

/* 0x46070, 183 bytes. Shift the nine 20-byte history records down and reset the newest slots. */
void shift_history_46070(void)
{
    uint32_t i;

    for (i = 0; i < 9; i++) {
        uint32_t dest = i * 0x14u;
        uint32_t source = (i + 1u) * 0x14u;

        EF_U16(dest + 0x00126690u) = EF_U16(source + 0x00126690u);
        EF_U16(dest + 0x00126692u) = EF_U16(source + 0x00126692u);
        EF_U32(dest + 0x00126698u) = EF_U32(source + 0x00126698u);
        EF_U32(dest + 0x0012669cu) = EF_U32(source + 0x0012669cu);
        EF_U32(dest + 0x001266a0u) = EF_U32(source + 0x001266a0u);
        EF_U32(dest + 0x00126694u) = EF_U32(source + 0x00126694u);
    }
    EF_U16(0x00126744u) = 0xff;
    EF_U16(0x00126746u) = 0xff;
    EF_U32(0x00126748u) = 0xffffffffu;
    EF_U32(0x0012674cu) = 0;
    EF_U32(0x00126750u) = 0;
    EF_U32(0x00126754u) = 0;
    if (((int32_t)EF_U32(0x0012668eu) >> 16) != 0xff)
        EF_U16(0x00126658u) = 0x6e;
}

/* 0x48064, 179 bytes. Advance the countdowns and age the four 140-entry timer tables. */
void update_timers_48064(void)
{
    uint16_t frame = EF_U16(0x00127d92u);
    uint16_t changed = 0;
    int16_t table;

    if (EF_U8(0x00126654u) != 0)
        tick_countdown_47f48();
    if (EF_U8(0x00126655u) != 0)
        tick_countdown_47fbc();
    frame = EF_U16(0x00127d92u);
    if (EF_U8(0x001a5228u) != 0) {
        for (table = 1; table < 4; table++) {
            int16_t i;

            for (i = 0; i < 0x8c; i++) {
                ef_addr32 cell = (ef_addr32)((int32_t)i * 2 + (int32_t)table * 0x190);

                if ((int16_t)EF_U16(cell + 0x001a4bb0u) > 0) {
                    int32_t step = (int16_t)EF_U16(cell + 0x001a4bb0u);

                    if (step > (int32_t)frame)
                        step = (int32_t)frame;
                    EF_U16(cell + 0x001a4bb0u) = (uint16_t)((int16_t)EF_U16(cell + 0x001a4bb0u) - step);
                    changed = 1;
                }
            }
        }
    }
    EF_U8(0x001a5228u) = changed != 0;
    EF_U16(0x00127d92u) = frame;
}

/* Shared tail of the two link-copy helpers: copy a unit's target link when the teams differ. */
static void copy_link_if_enemy(ef_addr32 source, ef_addr32 dest, ef_addr32 linked)
{
    ef_addr32 base;

    if (linked == 0)
        return;
    base = EF_U32(0x0019bb58u);
    if (EF_U16(base + (uint32_t)((int32_t)(int16_t)EF_U16(linked + 0x28u) * 0xca) + 0x15u) ==
        EF_U16(base + (uint32_t)((int32_t)(int16_t)EF_U16(source + 0x28u) * 0xca) + 0x15u))
        return;
    EF_U16(dest + 0x146u) = EF_U16(source + 0x146u);
}

/* 0x48798, 175 bytes. Set the target mode and share the link with an enemy-owned linked unit. */
void share_link_48798(int16_t first, int16_t second, uint8_t mode)
{
    ef_addr32 source = guest_fn_7489c((int32_t)first);
    ef_addr32 dest = guest_fn_7489c((int32_t)second);
    ef_addr32 linked = guest_fn_7489c((int32_t)(int16_t)EF_U16(source + 0x146u));

    if (mode == 1)
        set_indexed_object_mode(second, 4);
    if (mode == 0)
        set_indexed_object_mode(second, 5);
    if (linked == 0)
        return;
    if ((EF_U8(linked + 0x24u) & 4u) != 0 || (EF_U8(linked + 0x24u) & 8u) != 0)
        return;
    copy_link_if_enemy(source, dest, linked);
}

/* 0x48848, 175 bytes. Like 0x48798 with modes 6 and 7; the flag test comes first. */
void share_link_48848(int16_t first, int16_t second, uint8_t mode)
{
    ef_addr32 source = guest_fn_7489c((int32_t)first);
    ef_addr32 dest = guest_fn_7489c((int32_t)second);
    ef_addr32 linked = guest_fn_7489c((int32_t)(int16_t)EF_U16(source + 0x146u));

    if (mode == 1)
        set_indexed_object_mode(second, 6);
    if (mode == 0)
        set_indexed_object_mode(second, 7);
    if (linked == 0)
        return;
    if ((EF_U8(linked + 0x24u) & 4u) != 0 || (EF_U8(linked + 0x24u) & 8u) != 0)
        return;
    copy_link_if_enemy(source, dest, linked);
}

/* 0x54010, 191 bytes. Reset the flight-control state words and floats. */
void reset_flight_state_54010(void)
{
    EF_U8(0x001a5c0cu) = 0;
    EF_U16(0x00127d34u) = 1;
    EF_U16(0x00127d3cu) = 0;
    EF_U16(0x00127d3eu) = 0;
    EF_U16(0x00127d40u) = 0;
    EF_U32(0x00127d70u) = 0;
    EF_U32(0x00127d74u) = 0;
    EF_U8(0x00127d43u) = 0;
    EF_U8(0x00127d42u) = 0;
    EF_U8(0x001a364eu) = 0;
    EF_U32(0x00127d38u) = 0x3c03126fu;
    EF_U32(0x00127d64u) = 0;
    EF_U32(0x00127d68u) = 0;
    EF_U32(0x00127d6cu) = 0;
    EF_U32(0x00127d78u) = 0;
    EF_U32(0x00127d7cu) = 0;
    EF_U32(0x00127d80u) = 0;
    EF_U32(0x00127d84u) = 0;
    EF_U16(0x00127d8cu) = 0;
    EF_U32(0x00127d88u) = 0x3f800000u;
    EF_U8(0x001a363au) = 0;
    EF_U8(0x00127d9du) = 0xff;
}

/* 0x54410, 174 bytes. Reset every unit's team word, the owner flags and the 8 counters. */
void reset_unit_teams_54410(void)
{
    int16_t i;
    int16_t owner_count;

    for (i = 0; i < 0x898; i++) {
        ef_addr32 unit = guest_fn_7489c((int32_t)i);

        if (unit == 0)
            continue;
        if (EF_U8(0x001a666bu) == 0) {
            if (EF_U16(unit + 0x16u) == EF_U16(0x001f71cau))
                EF_U16(unit + 0x18u) = EF_U8(0x001a666bu);
            else
                EF_U16(unit + 0x18u) = 0xff;
        } else {
            EF_U16(unit + 0x18u) = 0;
        }
    }
    owner_count = (int16_t)EF_U16(0x0019bb5cu);
    if (owner_count > 0) {
        int16_t j = 0;

        do {
            EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)j * 0xca)) = 0;
            j++;
        } while (j < (int16_t)EF_U16(0x0019bb5cu));
    }
    for (i = 0; i < 8; i++) {
        EF_U16(0x001a6388u + (uint32_t)i * 2u) = 0;
        EF_U16(0x001a6378u + (uint32_t)i * 2u) = 0;
    }
}

/* 0x56af4, 179 bytes. Store an owner summary message: name, counters and flags. */
void apply_owner_summary_56af4(ef_addr32 message)
{
    int32_t owner = (int16_t)EF_U16(message + 1u);

    copy_string_9d749((uint32_t)(owner * 17) + 0x001a62f0u, message + 5u);
    owner = (int16_t)EF_U16(message + 1u);
    if (owner < 8) {
        if (EF_U8(message + 0x1eu) != 0 &&
            (guest_fn_7574c((int32_t)(int16_t)EF_U16(message + 3u)) & 0xffu) != 0)
            EF_U16(0x00126518u) = 1;
        owner = (int16_t)EF_U16(message + 1u);
        EF_U16(0x001a63b0u + (uint32_t)(owner * 2)) = EF_U16(message + 3u);
        owner = (int16_t)EF_U16(message + 1u);
        EF_U16(0x001a6388u + (uint32_t)(owner * 2)) = EF_U16(message + 0x16u);
        owner = (int16_t)EF_U16(message + 1u);
        EF_U16(0x001a6378u + (uint32_t)(owner * 2)) = EF_U16(message + 0x18u);
        owner = (int16_t)EF_U16(message + 1u);
        EF_U16(0x001a6398u + (uint32_t)(owner * 2)) = EF_U16(message + 0x1au);
        owner = (int16_t)EF_U16(message + 1u);
        EF_U8(0x001a63a8u + (uint32_t)owner) = (uint8_t)(EF_U8(message + 0x1cu) * 0xf4u);
    }
    if (EF_U8(0x001357c8u) != 0)
        EF_U16(0x001a6632u) = 0xc8;
}

/* 0x5f588, 172 bytes. Turn direction of three points: 1, -1 (0xff) or 0, with a collinear tiebreak. */
uint8_t turn_direction_5f588(ef_addr32 a, ef_addr32 b, ef_addr32 c)
{
    int32_t dx1 = (int16_t)(EF_U16(b) - EF_U16(a));
    int32_t dy1 = (int16_t)(EF_U16(b + 2u) - EF_U16(a + 2u));
    int32_t dx2 = (int16_t)(EF_U16(c) - EF_U16(a));
    int32_t dy2 = (int16_t)(EF_U16(c + 2u) - EF_U16(a + 2u));
    int32_t left = dy2 * dx1;
    int32_t right = dy1 * dx2;

    if (left > right)
        return 1;
    if (left < right)
        return 0xff;
    if (dx1 * dx2 < 0 || dy1 * dy2 < 0)
        return 0xff;
    if (dx1 * dx1 + dy1 * dy1 < dx2 * dx2 + dy2 * dy2)
        return 1;
    return 0;
}
