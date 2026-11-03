/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit-record predicates and small game-state helpers, 0x5fd8..0x1b52c. */

/* 0x5fd8, 67 bytes. Reinitialise the game screens. */
void reinit_screens_5fd8(void)
{
    guest_fn_121f0();
    guest_fn_12160();
    guest_fn_65960();
    guest_fn_5f634();
    guest_fn_5f8d4(EF_U8(0x00197256u));
    EF_U8(0x00197252u) = 0;
    clear_command_state_6bf4();
    guest_fn_14ef4();
    guest_fn_150d4();
    EF_U16(0x0019724eu) = 0;
}

/* 0x6b1c, 50 bytes. */
void update_flag_6b1c(void)
{
    uint8_t value;

    if (EF_U8(0x00127da9u) == 0)
        return;
    value = EF_U8(0x00127da8u);
    if (value != 0) {
        EF_U8(0x001971b4u) = 0;
        return;
    }
    if (EF_U8(0x001357c8u) == 0)
        EF_U8(0x001971b4u) = value;
}

/* 0xa9a8, 67 bytes. Smaller of (table byte + 1) and (random % 3 + 2). */
int32_t pick_count_a9a8(void)
{
    int32_t limit = (int32_t)(half_word_9c960() & 0xffffu) % 3 + 2;
    int32_t entry = (int32_t)EF_U8(0x001264e7u + (uint32_t)EF_U8(0x001264e2u) * 5u) + 1;

    return entry < limit ? entry : limit;
}

/* 0xb544, 46 bytes. */
void apply_two_a4b80_b544(void)
{
    guest_fn_a4b80(EF_U8(guest_fn_a46cc(0x001805fcu) + 0x10u));
    guest_fn_a4b80(EF_U8(guest_fn_a46cc(0x001805f0u) + 0x10u));
}

/* 0xef5c, 58 bytes. Fill four dwords with 21-bit values scaled by 2^11. */
uint32_t fill_random_ef5c(ef_addr32 destination)
{
    guest_fn_e810();
    EF_U32(destination) = guest_fn_e95c() << 11;
    EF_U32(destination + 4u) = guest_fn_e95c() << 11;
    EF_U32(destination + 8u) = guest_fn_e95c() << 11;
    EF_U32(destination + 0xcu) = guest_fn_e95c() << 11;
    return 1;
}

/* 0x13950, 55 bytes. */
void run_12d90_pair_13950(void)
{
    uint8_t saved = EF_U8(0x0012b4c4u);

    EF_U8(0x0012b4c4u) = 0;
    guest_fn_12d90(3, 1);
    guest_fn_12d90(4, 1);
    EF_U8(0x0012b4c4u) = saved;
}

/* Address of unit record index (0xca bytes each). */
static ef_addr32 unit_record(uint32_t index)
{
    return EF_U32(0x0019bb58u) + index * 0xcau;
}

/* 0x16984, 45 bytes. Is the unit alive and not of kind 0xc? */
uint32_t unit_active_16984(uint32_t index)
{
    ef_addr32 unit = unit_record(index);

    return EF_U8(unit + 0x4fu) != 0 && EF_U8(unit + 0x1au) != 0xc;
}

/* 0x16a04, 56 bytes. */
uint32_t unit_kind1_with_target_16a04(uint32_t index)
{
    ef_addr32 unit;

    if ((unit_active_16984(index) & 0xffffu) == 0)
        return 0;
    unit = unit_record(index);
    return EF_U8(unit + 0x1bu) == 1u && EF_U32(unit + 0xb5u) != 0;
}

/* 0x16a3c, 55 bytes. */
uint32_t unit_kind_6_or_7_16a3c(uint32_t index)
{
    uint8_t kind = EF_U8(unit_record(index) + 0x1bu);

    if ((unit_active_16984(index) & 0xffffu) == 0)
        return 0;
    return kind == 6 || kind == 7;
}

/* 0x16a74, 64 bytes. */
uint32_t unit_kind_b_or_flag_16a74(uint32_t index)
{
    if ((unit_active_16984(index) & 0xffffu) == 0)
        return 0;
    if (EF_U8(unit_record(index) + 0x1bu) == 0xb)
        return 1;
    return (guest_fn_173c4(index) & 0xffffu) != 0;
}

/* 0x17434, 63 bytes. Search the 20 three-byte slots of a unit for kind 6 or 0x15. */
uint32_t find_slot_kind_17434(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    ef_addr32 slot;
    uint32_t i;

    if (unit == 0)
        return 0;
    slot = unit + 0x19bu;
    for (i = 0; i < 0x14u; i++, slot += 3u) {
        uint16_t kind = (uint16_t)(int16_t)(int8_t)EF_U8(slot);

        if (kind == 6u)
            return 6;
        if (kind == 0x15u)
            return (unit & 0xffff0000u) | 0x15u; /* only AX is set by the original */
    }
    return 0;
}

/* 0x174c8, 47 bytes. Does the unit's target differ from value and the unit's kind allow it? */
uint32_t unit_target_differs_174c8(uint32_t index, int32_t value)
{
    if ((int32_t)(EF_U32(unit_record(index) + 0x13u) >> 16) == value)
        return 0;
    return unit_kind_allows_174f8(index) != 0;
}

/* 0x174f8, 62 bytes. Is the unit's kind not one of 0, 7, 9, 0xa, 0xb, 0xc? */
uint32_t unit_kind_allows_174f8(uint32_t index)
{
    uint8_t kind = EF_U8(unit_record(index) + 0x1au);

    return !(kind == 0xc || kind == 0xa || kind == 0xb || kind == 7 || kind == 9 || kind == 0);
}

/* 0x1b52c, 42 bytes. */
uint32_t selected_unit_visible_1b52c(void)
{
    int32_t id = EF_S16(0x001a823cu);
    ef_addr32 unit;

    if (id == 0xff)
        return 0;
    unit = guest_fn_7489c(id);
    if (unit == 0 || (EF_U8(unit + 0x24u) & 8u) != 0)
        return 0;
    return 1;
}
