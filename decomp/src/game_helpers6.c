/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit-record, string-pool and UI-state helpers, 0x624f4..0x9ab30. */

/* 0x624f4, 16 bytes. */
void refresh_all_624f4(void)
{
    guest_fn_6454c();
    guest_fn_62edc();
    guest_fn_62b84();
}

/* 0x7556c, 90 bytes. Return the pointer or value associated with the unit's slot. */
uint32_t unit_slot_value_7556c(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    uint8_t slot;

    if (unit == 0)
        return 0;
    if (EF_U16(0x0019bb5eu) == EF_U16(unit + 0x28u)) {
        if (unit + 0x2bu != 0)
            return EF_U32(unit + 0x3fu);
        slot = EF_U8(unit + 0x2au);
    } else {
        slot = EF_U8(unit + 0x2au);
    }
    if (slot == 0)
        return 0;
    return (uint32_t)slot * 8u - 8u + 0x00126a44u + 4u;
}

/* 0x7632c, 68 bytes. Copy the 3-byte payloads of the 4-byte entries at 0x421750 until 0xff. */
void expand_table_7632c(void)
{
    int16_t i = 0;

    for (;;) {
        ef_addr32 entry = 0x00421750u + (uint32_t)(int32_t)i;
        uint32_t slot = EF_U8(entry);

        if (slot == 0xffu)
            return;
        slot *= 3u;
        EF_U8(0x001f71e0u + slot) = EF_U8(entry + 1u);
        EF_U8(0x001f71e1u + slot) = EF_U8(entry + 2u);
        EF_U8(0x001f71e2u + slot) = EF_U8(entry + 3u);
        i = (int16_t)(i + 4);
    }
}

/* 0x7a51c, 46 bytes. Append a string to the string pool at 0x1f8e94. */
void pool_append_7a51c(ef_addr32 text)
{
    copy_string_9d749(EF_U32(0x001f8e94u), text);
    EF_U32(0x001f8e94u) = EF_U32(0x001f8e94u) + guest_fn_b1b8b(text) + 1u;
    guest_fn_7a480();
}

/* 0x7a54c, 54 bytes. Copy the pool string into 0x1f8f14 and advance the pool. */
ef_addr32 pool_take_7a54c(void)
{
    copy_string_9d749(0x001f8f14u, EF_U32(0x001f8e94u));
    EF_U32(0x001f8e94u) = EF_U32(0x001f8e94u) + guest_fn_b1b8b(0x001f8f14u) + 1u;
    guest_fn_7a480();
    return 0x001f8f14u;
}

/* 0x7a5bc, 50 bytes. */
void flush_pool_7a5bc(void)
{
    if (EF_U32(0x0012afc8u) != 0)
        return;
    guest_fn_7a3b0(0x00187ba8u, EF_U32(0x001f8e90u), EF_U32(0x001f8e94u) - EF_U32(0x001f8e90u));
    guest_fn_b3d00();
}

/* 0x7b3c8, 49 bytes. Allocate the two 0x27100-byte buffers once. */
void alloc_buffers_7b3c8(void)
{
    if (EF_U32(0x001f8eb8u) == 0)
        EF_U32(0x001f8eb8u) = guest_fn_e29e8(0x27100u);
    if (EF_U32(0x001f8eb4u) == 0)
        EF_U32(0x001f8eb4u) = guest_fn_e29e8(0x27100u);
}

/* 0x7eec4, 50 bytes. */
void reset_menu_7eec4(void)
{
    ef_addr32 menu = EF_U32(0x001f9078u);

    EF_U8(0x001f9094u) = 0;
    EF_U8(0x001f9098u) = 0;
    if (menu != 0)
        guest_fn_d76f4(EF_U32(menu + 0x58u), 0x5622);
    guest_fn_802ac(0x16);
}

/* 0x802e0, 52 bytes. */
void show_menu_item_802e0(int16_t item)
{
    if (EF_U8(0x0012b070u) == 0 || item == 0x47)
        return;
    guest_fn_7fbec(0xa, 0, (uint32_t)(int32_t)item,
                   guest_fn_d771c(EF_U32(EF_U32(0x001f9078u) + (uint32_t)((int32_t)item * 4))));
}

/* 0x80314, 62 bytes. */
void show_menu_item_80314(void)
{
    if (EF_U8(0x0012b070u) == 0 || EF_U32(0x0012b07cu) == 0)
        return;
    if ((guest_fn_d7594(0xd) & 0xffu) == 0)
        return;
    guest_fn_7fbec(0xd, 0, 0xf, EF_U32(0x0012b07cu));
}

/* 0x815b4, 64 bytes. Find the 8-byte entry whose two words match those at pointer. */
ef_addr32 find_entry_815b4(ef_addr32 key)
{
    uint32_t i;

    if (EF_U8(0x0012b104u) == 0)
        return 0;
    for (i = 0; (int32_t)i < 0x40; i += 8u) {
        if (EF_U16(key) == EF_U16(0x0012b16au + i) && EF_U16(key + 2u) == EF_U16(0x0012b168u + i))
            return 0x0012b168u + i + 4u;
    }
    return 0;
}

/* 0x8461c, 52 bytes. */
uint8_t advance_state_8461c(void)
{
    if (EF_U16(0x0012b4a6u) == 2u) {
        EF_U16(0x0012b4a6u) = 3;
        guest_fn_dc832(0x001fcbd4u, 0x001fcc2cu, 0xc);
    }
    return 0;
}

/* 0x84a80, 60 bytes. Does the unit own a slot of kind 0x21 or 0x22? */
uint8_t unit_has_slot_84a80(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    int16_t i;

    if (unit == 0)
        return 0;
    for (i = 0; i < 0x14; i++) {
        int32_t kind = (int32_t)EF_U32(unit + (uint32_t)((int32_t)i * 3) + 0x198u) >> 24;

        if (kind == 0x22 || kind == 0x21)
            return 1;
    }
    return 0;
}

/* 0x85b70, 63 bytes. */
void reset_flags_85b70(void)
{
    EF_U8(0x001fcc60u) = 0;
    EF_U8(0x001fcc61u) = 1;
    EF_U8(0x001fcc63u) = 0;
    EF_U8(0x001fcc64u) = 0;
    EF_U8(0x001fcc65u) = 0;
    EF_U8(0x001fcc66u) = 0;
    EF_U8(0x001fcc67u) = 0;
    EF_U8(0x001fcc68u) = 0;
    EF_U8(0x001fcc62u) = 2;
}

/* 0x880c8, 53 bytes. Run 878c8 for every ring node of kind 1. */
void process_ring_880c8(void)
{
    ef_addr32 node = EF_U32(EF_U32(0x0020f160u) + 0x34u);

    while (node != EF_U32(0x0020f164u)) {
        ef_addr32 next = EF_U32(node + 0x34u);

        if (EF_U8(node + 0x42u) == 1u)
            guest_fn_878c8(node);
        node = next;
    }
}

/* 0x8965c, 59 bytes. */
void activate_8965c(ef_addr32 object)
{
    EF_U8(0x0012d7b3u) = 1;
    EF_U8(0x0020f22fu) = 0;
    EF_U8(object + 0x58u) = 1;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S16(object + 0x2eu), 3);
    guest_fn_8a318(object);
}

/* 0x89990, 69 bytes. */
void activate_89990(ef_addr32 object)
{
    uint32_t kind;

    EF_U8(0x0012d7b3u) = 1;
    EF_U8(object + 0x58u) = 1;
    kind = EF_U8(object + 0x18u);
    if (kind == 5 || kind == 8)
        EF_U8(object + 0x58u) = 0;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S16(object + 0x2eu), 3);
    guest_fn_8a318(object);
}

/* 0x8c064, 55 bytes. Is value present in the 0x898-entry table at 0x20f550? */
uint8_t table_contains_8c064(uint32_t value)
{
    int16_t i;

    for (i = 0; i < 0x898; i++) {
        if (EF_U32(0x0020f550u + (uint32_t)((int32_t)i * 4)) == value)
            return 1;
    }
    return 0;
}

/* 0x8d554, 70 bytes. */
void maybe_retarget_8d554(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);

    if (unit == 0 || EF_U8(unit + 0x16cu) != 0xe)
        return;
    if (EF_U16(unit + 0x18u) != (uint16_t)EF_U8(0x001a666bu) && EF_S16(unit + 0x18u) != 0xff)
        return;
    guest_fn_725dc(unit_id);
}

/* 0x94880, 68 bytes. */
void update_view_94880(ef_addr32 argument)
{
    ef_addr32 unit = guest_fn_7489c(EF_S16(0x001f71cau));

    if ((guest_fn_9125c(EF_S16(unit + 0x20u)) & 0xffffu) != 0)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c(EF_S16(unit + 0x20u));
    guest_fn_9457c(unit, argument);
}

/* 0x98280, 31 bytes. */
void reset_mode_98280(void)
{
    guest_fn_980e8(EF_U8(0x0012dd0du));
    EF_U8(0x0012dd0cu) = 0;
    EF_U8(0x0012dd0du) = 4;
}

/* 0x982a0, 50 bytes. */
void select_handlers_982a0(void)
{
    uint8_t value = EF_U8(0x00420a72u);

    EF_U8(0x00420a70u) = value;
    if (value == 0 || (guest_fn_ddbe0() & 0xffu) == 0) {
        EF_U32(0x0012dd04u) = 0xde578u;
        EF_U32(0x0012dd08u) = 0xde57cu;
    }
}

/* 0x984f0, 25 bytes. Allocate size bytes and clear them. */
ef_addr32 alloc_zeroed_984f0(uint32_t size)
{
    ef_addr32 block = guest_fn_e29e8(size);

    guest_fn_b3fb0(block, 0, size);
    return block;
}

/* 0x9bb64, 64 bytes. Shift the three 20-entry word tables of a record down by one. */
void shift_tables_9bb64(ef_addr32 record)
{
    int16_t i;

    for (i = 1; i < 0x14; i++) {
        uint32_t offset = (uint32_t)(int32_t)i * 2u;

        EF_U16(record + offset - 2u) = EF_U16(record + offset);
        EF_U16(record + offset + 0x26u) = EF_U16(record + offset + 0x28u);
        EF_U16(record + offset + 0x4eu) = EF_U16(record + offset + 0x50u);
    }
}
