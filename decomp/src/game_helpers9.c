/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network-message, unit-slot and random-table helpers, 0x57218..0x8018c. */

/* 0x57218, 116 bytes. */
void handle_owner_message_57218(ef_addr32 message)
{
    ef_addr32 unit;
    ef_addr32 record;

    if (EF_U8(message + 1u) != EF_U8(0x001a666bu) || EF_S16(message + 2u) == 0xff)
        return;
    unit = guest_fn_7489c(EF_S16(message + 2u));
    if (unit == 0)
        return;
    record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_S16(unit + 0x28u) * 0xca);
    EF_U8(record + 0xc9u) = EF_U8(message + 4u);
    guest_fn_46168(EF_S16(unit + 0x16u), 0x23, EF_U8(record + 0xc9u), 0, 0);
}

/* 0x5736c, 100 bytes. Clear slot message[3] of the owner's table. */
void clear_owner_slot_5736c(ef_addr32 message)
{
    uint8_t slot = EF_U8(message + 3u);
    ef_addr32 unit = guest_fn_7489c(EF_S16(message + 1u));

    if (unit == 0)
        return;
    if ((uint16_t)EF_U8(0x001a666bu) != EF_U16(unit + 0x18u) && EF_S16(unit + 0x18u) != 0xff)
        return;
    EF_U16(EF_U32(0x001f8918u) + (uint32_t)EF_U8(unit + 0x187u) * 25u + (uint32_t)slot * 2u + 0x13u) = 0xff;
}

/* 0x58028, 107 bytes. Rebuild the bit mask of the slots 1..count-1. */
void rebuild_slot_mask_58028(void)
{
    int16_t count;
    int16_t i;
    uint32_t mask = 0;

    EF_U8(0x00135799u) = 0;
    guest_fn_56004();
    guest_fn_56208();
    count = EF_S16(0x001357b0u);
    for (i = 1; i < count; i++)
        mask |= 1u << (uint8_t)i;
    EF_U16(0x001a6632u) = 0;
    EF_U16(0x00127dacu) = 0;
    EF_U16(0x001357b0u) = (uint16_t)count;
    EF_U16(0x001a662eu) = (uint16_t)mask;
}

/* 0x653d8, 53 bytes. Find the node with this kind and id in the list at 0x1a7c54. */
ef_addr32 find_node_653d8(uint32_t kind, uint32_t id)
{
    ef_addr32 node;

    for (node = EF_U32(0x001a7c54u); node != 0; node = EF_U32(node + 0x16u)) {
        if ((uint16_t)EF_U8(node + 0xcu) == (uint16_t)kind && EF_U16(node + 0xeu) == (uint16_t)id)
            return node;
    }
    return 0;
}

/* 0x685ac, 107 bytes. Fill 300 entries of three words with sums of two random values. */
void fill_random_table_685ac(void)
{
    int16_t i;

    for (i = 0; i < 0x12c; i++) {
        uint32_t entry = (uint32_t)((int32_t)i * 6);
        uint32_t a = guest_fn_a45b3();
        uint32_t b = guest_fn_a45b3();
        uint32_t c;
        uint32_t d;

        EF_U16(0x001a8590u + entry) = (uint16_t)(a + b);
        c = guest_fn_a45b3();
        d = guest_fn_a45b3();
        EF_U16(0x001a8592u + entry) = (uint16_t)(c + d);
        a = guest_fn_a45b3();
        b = guest_fn_a45b3();
        EF_U16(0x001a8594u + entry) = (uint16_t)(a + b);
    }
}

/* 0x6900c, 115 bytes. */
void reset_view_6900c(void)
{
    int16_t i;

    EF_U8(0x0012b024u) = 0;
    EF_U32(0x001282e0u) = 0xffffffffu;
    EF_U16(0x001f6f3cu) = 0x14f0;
    EF_U16(0x001f6f3eu) = 0x8ed0;
    EF_U16(0x001f6f4au) = 0;
    EF_U16(0x001f6f4cu) = 0;
    EF_U16(0x001f6f48u) = 0x320;
    for (i = 0; i < 0x14; i++)
        EF_U8(0x001f6ef4u + (uint32_t)((int32_t)i * 3)) = 0;
}

/* 0x699f4, 73 bytes. Index of the 0x91-byte record whose name equals text, or 0. */
uint32_t find_named_record_699f4(ef_addr32 text)
{
    uint32_t index = 2;
    uint32_t offset = 0x122;

    if (EF_U32(0x0012bd8au) == 0)
        return 0;
    for (;;) {
        if (guest_fn_9d524(EF_U32(0x0012bc68u + offset), text) == 0)
            return index;
        offset += 0x91u;
        index++;
        if (EF_U32(0x0012bcf9u + offset - 0x91u) == 0)
            return 0;
    }
}

/* 0x6a148, 100 bytes. Next unit after start that belongs to a record of kind 1, or 0xff. */
uint32_t next_unit_6a148(uint32_t start)
{
    uint32_t cursor = start;
    ef_addr32 unit;

    do {
        cursor++;
        if ((int16_t)cursor >= (int16_t)EF_U16(0x002119f4u))
            cursor = 0;
        unit = guest_fn_7489c((int16_t)cursor);
        if (unit != 0) {
            ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_S16(unit + 0x28u) * 0xca);

            if (EF_U8(record + 0x20u) != 1u)
                unit = 0;
        }
    } while (unit == 0 && (uint16_t)cursor != (uint16_t)start);
    if (unit == 0)
        cursor = 0xff;
    return cursor;
}

/* 0x752d0, 113 bytes. Copy the waypoint selected by the unit's index into the unit. */
void load_waypoint_752d0(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    ef_addr32 list;
    ef_addr32 node;
    uint32_t position = 0;
    uint32_t index;

    if (unit == 0)
        return;
    list = EF_U32(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_S16(unit + 0x28u) * 0xca) + 0xbdu);
    if (list == 0)
        return;
    node = EF_U32(list);
    if (node != 0) {
        index = EF_U8(unit + 0x2au);
        while ((int32_t)position < (int32_t)index) {
            node = EF_U32(node + 4u);
            position++;
            if (node == 0)
                break;
        }
    }
    if (node == 0)
        return;
    EF_U32(unit + 0x1e4u) = EF_U32(node);
    EF_U32(unit + 0x201u) = EF_U32(node + 8u);
    EF_U32(unit + 0x205u) = EF_U32(node + 0xcu);
}

/* 0x755c8, 85 bytes. Sum the weights of the unit's slots whose class equals filter. */
uint32_t sum_slot_weights_755c8(int16_t unit_id, uint8_t filter)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    uint32_t total = 0;
    uint8_t i;

    if (unit == 0)
        return 0;
    for (i = 0; i < 0x13; i++) {
        ef_addr32 slot = unit + (uint32_t)i * 3u;
        int32_t kind = (int32_t)EF_U32(slot + 0x198u) >> 24;

        if (EF_U8(0x0012bc94u + (uint32_t)(kind * 0x91)) == filter)
            total = (total & 0xffff0000u) | (uint16_t)(total + EF_U16(slot + 0x19cu));
    }
    return total;
}

/* 0x77088, 74 bytes. */
void assign_slot_77088(ef_addr32 unit, uint8_t slot)
{
    ef_addr32 record = EF_U32(0x001f8918u) + (uint32_t)EF_U8(unit + 0x187u) * 25u;
    int16_t value = EF_S16(record + 3u);

    EF_U16(record + (uint32_t)slot * 2u + 0x13u) = (uint16_t)value;
    guest_fn_77034(EF_S16(unit + 0x16u), value, slot);
    guest_fn_76f58(unit, record);
}

/* 0x7ae0c, 123 bytes. Read the 16-bit value at wrapped (a, b) of the 0x190x0x190 two-plane map. */
uint32_t map_value_7ae0c(int32_t a, int32_t b)
{
    int32_t column = (a >> 11) % 0x190;
    int32_t row = 0x18f - (b >> 11);
    uint32_t offset;

    if (column < 0)
        column += 0x18f;
    row %= 0x190;
    if (row < 0)
        row += 0x18f;
    offset = (uint32_t)column * 400u + (uint32_t)row;
    return ((uint32_t)EF_U8(EF_U32(0x001f8eb4u) + offset) << 8) | EF_U8(EF_U32(0x001f8eb8u) + offset);
}

/* 0x7f7dc, 115 bytes. Squared distance between a vector (>>8, 16-bit) and the origin at 0x21444c. */
uint32_t squared_distance_7f7dc(ef_addr32 vector)
{
    int16_t dx;
    int16_t dy;
    int16_t dz;

    if (vector == 0)
        return vector;
    dx = (int16_t)((EF_U32(0x0021444cu) & 0xffffu) - (((int32_t)EF_U32(vector) >> 8) & 0xffffu));
    dy = (int16_t)((EF_U32(0x00214448u) & 0xffffu) - (((int32_t)EF_U32(vector + 4u) >> 8) & 0xffffu));
    dz = (int16_t)((EF_U32(0x00214450u) & 0xffffu) - (((int32_t)EF_U32(vector + 8u) >> 8) & 0xffffu));
    return (uint32_t)((int32_t)dy * dy + (int32_t)dx * dx + (int32_t)dz * dz);
}

/* 0x8018c, 87 bytes. */
void show_menu_value_8018c(int16_t item, uint32_t value)
{
    uint8_t kind;
    uint32_t clamped;

    if (EF_U8(0x0012b070u) == 0)
        return;
    kind = (uint8_t)guest_fn_d70a0(read_field_d7738(EF_U32(EF_U32(0x001f9078u) + (uint32_t)((int32_t)item * 4))) & 0xffu);
    if (kind == 0xe)
        return;
    clamped = value < 0x7fu ? value : 0x7fu;
    guest_fn_7fbec(kind, 1, (uint32_t)(int32_t)item, clamped);
}
