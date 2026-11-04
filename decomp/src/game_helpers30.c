/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Timers, record initialisers and 10-byte entry list helpers, scattered. */

/* 0x6330, 81 bytes. Count down a timer for an active object; call 0x56a4c and rewind on underflow. */
void tick_object_timer_6330(ef_addr32 object)
{
    uint8_t flags;
    int16_t value;

    if (object == 0 || EF_U16(0x00126518u) != 0)
        return;
    flags = EF_U8(object + 0x24u);
    if ((flags & 4u) != 0 || (flags & 8u) != 0)
        return;
    if (EF_U8(0x00127da9u) == 0)
        return;
    value = (int16_t)(EF_U16(0x00121e4cu) - EF_U16(0x00127d92u));
    EF_U16(0x00121e4cu) = (uint16_t)value;
    if (value < 0) {
        guest_fn_56a4c();
        EF_U16(0x00121e4cu) = (uint16_t)(EF_U16(0x00121e4cu) + 0x32u);
    }
}

/* 0x47f48, 83 bytes. Decrease the countdown word, by a third of the frame time when flagged. */
void tick_countdown_47f48(void)
{
    if (EF_U8(0x001a3647u) == 0) {
        EF_U16(0x00126656u) = (uint16_t)(EF_U16(0x00126656u) - EF_U16(0x00127d92u));
    } else {
        int32_t third = (int32_t)EF_U16(0x00127d92u) / 3;

        EF_U16(0x00126656u) = (uint16_t)(((int32_t)EF_U32(0x00126654u) >> 16) - third);
    }
    if ((int16_t)EF_U16(0x00126656u) < 0)
        reset_pointer_126688();
}

/* 0x49cd4, 86 bytes. Copy the row words selected by an object's count into the 0x1a5280 list. */
void copy_row_words_49cd4(ef_addr32 object)
{
    ef_addr32 rows = EF_U32(0x001fcb40u);
    int16_t source = 0;
    uint32_t dest = 0;

    while (source < (int16_t)EF_U8(object + 0x12u)) {
        int32_t row = (int32_t)EF_U32(0x00125833u) >> 16;
        ef_addr32 entry = (ef_addr32)(row * 0x11) + rows + (uint32_t)((int32_t)source * 2);

        EF_U16(0x001a5280u + (uint32_t)(int32_t)(int16_t)dest * 2u) = EF_U16(entry + 1u);
        source++;
        dest++;
    }
    EF_U16(0x001a53c0u) = (uint16_t)dest;
    EF_U32(0x001fcb40u) = rows;
}

/* 0x8ccec, 90 bytes. Reset a unit's link slots to 0xff. */
void reset_unit_links_8ccec(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    uint32_t i;

    if (unit == 0)
        return;
    EF_U16(unit + 0x143u) = 0xff;
    EF_U8(unit + 0x12au) = 1;
    for (i = 0; i < 8; i++)
        EF_U16(unit + 0x12bu + i * 2u) = 0xff;
    EF_U16(unit + 0x13bu) = 0xff;
    EF_U8(unit + 0x13du) = 3;
    EF_U8(unit + 0x13eu) = 8;
    EF_U16(unit + 0x13bu) = 0;
}

/* 0x9d114, 90 bytes. Allocate and initialise a 0x56c-byte stream record bound to an owner. */
ef_addr32 new_stream_9d114(uint32_t owner)
{
    ef_addr32 record = guest_fn_9d060(0x56cu);

    EF_U32(record + 0x55cu) = 0;
    EF_U32(record + 0x558u) = 0;
    EF_U16(record + 0x556u) = 0;
    EF_U32(record + 0x552u) = 0;
    EF_U8(record) = 0;
    EF_U8(record + 0x400u) = 0;
    EF_U32(record + 0x564u) = 0;
    EF_U32(record + 0x568u) = 0x0009d084u;
    EF_U32(record + 0x560u) = owner;
    return record;
}

/* 0xa4e14, 86 bytes. Rebuild the id table from the objects that are flagged and not kind 1. */
void rebuild_id_table_a4e14(void)
{
    uint16_t count = EF_U16(0x0041eed8u);
    uint16_t id;
    uint32_t out = 0;

    for (id = 1; id <= count; id++) {
        ef_addr32 object = EF_U32(0x0041edfcu + (uint32_t)id * 4u);

        if (EF_U8(object + 0xc1u) != 1 && (EF_U8(object + 0x3bu) & 4u) != 0) {
            EF_U16(0x0041eda8u + (uint32_t)(uint16_t)out * 2u) = id;
            out++;
        }
    }
    EF_U16(0x0041eed8u) = count;
}

/* 0xa7204, 90 bytes. Walk a chain of children and tidy each one that is not a basic kind. */
void tidy_chain_a7204(ef_addr32 object)
{
    ef_addr32 node = EF_U32(object + 0x91u);

    while (node != 0) {
        uint8_t kind = (uint8_t)(EF_U8(node + 0x8cu) & 0x1fu);
        int skip = kind == 9 || kind == 1 || kind == 2 || kind == 3;

        if (!skip && (kind == 0x0b || kind == 0x0e) &&
            EF_U16(EF_U32(node + 0x51u) + 0xdau) == 0)
            skip = 1;
        if (!skip)
            guest_fn_a6d00(node);
        if (EF_U32(node + 0x21u) == 0)
            EF_U16(EF_U32(node + 0x51u) + 0xdau) = 0;
        node = EF_U32(node + 0x21u);
    }
}

/* 0xc3a74, 79 bytes. Choose the drawing handlers from two mode flags. */
void select_draw_handlers_c3a74(void)
{
    EF_U32(0x0015b74cu) = 0x000c3f1cu;
    EF_U32(0x0015b750u) = 0x000c399cu;
    EF_U32(0x0041f3d0u) = 0x000e9a24u;
    if (EF_U8(0x00146e49u) != 0)
        EF_U32(0x0041f3d0u) = 0x000fc3d0u;
    if (EF_U8(0x00146e4au) != 0)
        EF_U32(0x0015b74cu) = 0x000c3a70u;
}

/* 0xf8988, 85 bytes. Find the last entry that is set and fails its check, clear it; report if found. */
uint32_t clear_failed_entry_f8988(ef_addr32 object)
{
    int16_t i = (int16_t)(EF_U8(object + 4u) - 1);
    uint32_t found = 0;

    while (i >= 0 && (uint16_t)found == 0) {
        uint32_t offset = (uint32_t)((int32_t)i * 10);

        if (EF_U16(EF_U32(object + 5u) + offset) != 0 &&
            (check_entry_f88a0(object, i) & 0xffffu) == 0) {
            EF_U16(EF_U32(object + 5u) + offset) = (uint16_t)found;
            found = 1;
        }
        i--;
    }
    return found;
}

/* 0xf89e0, 79 bytes. Count matching entries of a kind-0x15 list; 1 for other kinds. */
uint32_t count_kind_entries_f89e0(ef_addr32 object)
{
    uint32_t count = 0;
    int16_t i;

    if (EF_U8(object + 1u) != 0x15)
        return 1;
    for (i = 0; i < (int16_t)EF_U8(object + 4u); i++) {
        uint32_t code = EF_U8(EF_U32(object + 5u) + (uint32_t)((int32_t)i * 10) + 9u);

        if ((guest_fn_bd4e0(code) & 0xffffu) != 0)
            count++;
    }
    return count;
}

/* 0xf8a30, 69 bytes. Count the filled entries whose code passes the 0xbd4e0 test. */
uint32_t count_filled_entries_f8a30(ef_addr32 object)
{
    uint32_t count = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(object + 4u); i++) {
        uint32_t offset = (uint32_t)((int32_t)i * 10);
        uint32_t code = EF_U8(EF_U32(object + 5u) + offset + 9u);

        if ((guest_fn_bd4e0(code) & 0xffffu) != 0 &&
            EF_U16(EF_U32(object + 5u) + offset) != 0)
            count++;
    }
    return count;
}

/* 0xf8acc, 71 bytes. Decrement the last set entry that accepts it; report if one did. */
uint32_t decrement_last_entry_f8acc(ef_addr32 object)
{
    int16_t i = (int16_t)(EF_U8(object + 4u) - 1);
    uint32_t done = 0;

    while (i >= 0 && (uint16_t)done == 0) {
        ef_addr32 entry = EF_U32(object + 5u) + (uint32_t)((int32_t)i * 10);

        if (EF_U16(entry) != 0 && (decrement_f8a78(entry) & 0xffffu) != 0)
            done = 1;
        i--;
    }
    return done;
}

/* 0xf8e14, 47 bytes. Count the entries whose first word is nonzero. */
uint32_t count_set_entries_f8e14(ef_addr32 object)
{
    uint32_t count = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(object + 4u); i++) {
        if (EF_U16(EF_U32(object + 5u) + (uint32_t)((int32_t)i * 10)) != 0)
            count++;
    }
    return count;
}

/* 0xfabf0, 87 bytes. 1 when the object has at least one active entry whose code passes the set test. */
int32_t has_active_entry_fabf0(ef_addr32 object)
{
    int16_t found = 0;
    int16_t i = 0;

    if ((int16_t)EF_U16(object + 0x2au) > 0) {
        do {
            uint32_t offset = (uint32_t)((int32_t)i * 0x15);
            ef_addr32 base = EF_U32(object + 0x2cu);

            if ((is_active_bec84(base + offset) & 0xffffu) != 0 &&
                (code_in_set_bcb50(EF_U8(base + offset)) & 0xffffu) != 0)
                found++;
            i++;
        } while (i < (int16_t)EF_U16(object + 0x2au));
    }
    if (found > 1)
        return 1;
    return found;
}
