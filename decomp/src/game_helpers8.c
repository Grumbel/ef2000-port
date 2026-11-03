/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Line reader, grid, table-reset and message handlers, 0x3c558..0x562d8. */

/* 0x3c558, 93 bytes. Copy the next CR-terminated line from the input buffer to 0x1a2df0. */
void read_line_3c558(void)
{
    ef_addr32 source = EF_U32(0x001a2e6cu);
    int32_t remaining = EF_S32(0x001a2e70u);
    ef_addr32 destination = 0x001a2df0u;

    while (remaining != 0) {
        uint8_t c;

        remaining--;
        if (remaining == -1)
            break;
        c = EF_U8(source);
        if (c == 0xd)
            break;
        EF_U8(destination++) = c;
        source++;
    }
    if (EF_U8(source) == 0xd)
        source++;
    if (EF_U8(source) == 0xa)
        source++;
    EF_U8(destination) = 0;
    EF_U32(0x001a2e6cu) = source;
    EF_U32(0x001a2e70u) = (uint32_t)remaining;
}

/* 0x3c94c, 69 bytes. Store a 16-bit value split over two byte planes at (row, 0x18f - column). */
void store_plane_pixel_3c94c(int16_t row, int16_t column, int16_t value)
{
    uint32_t offset = (uint32_t)((int32_t)row * 25) << 4;
    uint32_t index = 0x18fu - (uint32_t)(int32_t)column;

    EF_U8(EF_U32(0x001f8eb8u) + offset + index) = (uint8_t)value;
    EF_U8(EF_U32(0x001f8eb4u) + offset + index) = (uint8_t)((int32_t)value >> 8);
}

/* 0x3d904, 73 bytes. Draw the 16x16 grid of 10x10 cells. */
void draw_grid_3d904(void)
{
    int16_t i;

    for (i = 0; i < 0x100; i++) {
        int16_t x = (int16_t)((i & 0xf) * 10);
        int16_t y = (int16_t)((i >> 4) * 10);

        guest_fn_ceccc(i, x, y, x + 9, y + 9);
    }
}

/* 0x3da24, 92 bytes. Are all entries of the table at 0x12b268 satisfied? */
uint8_t all_entries_ready_3da24(void)
{
    uint16_t count = EF_U16(0x001a5a86u);
    uint16_t ready = 0;
    uint16_t i;

    if (count == 0)
        return 0;
    for (i = 0; (int16_t)i < (int16_t)count; i++) {
        if ((guest_fn_a0e98(EF_S16(0x0012b268u + (uint32_t)i * 0x1au)) & 0xffu) != 0)
            ready++;
    }
    return ready == EF_U16(0x001a5a86u);
}

/* 0x45694, 82 bytes. */
void reset_words_45694(void)
{
    EF_U16(0x00126554u) = 1;
    EF_U16(0x00126556u) = 1;
    EF_U16(0x00126558u) = 1;
    EF_U16(0x0012655au) = 0;
    EF_U16(0x0012655cu) = 1;
    EF_U16(0x0012655eu) = 0;
    EF_U16(0x00126560u) = 0;
    EF_U16(0x00126562u) = 0;
    EF_U16(0x00126564u) = 0;
    EF_U16(0x0012656au) = 1;
}

/* 0x45ba0, 91 bytes. Reset the 10-entry id table and the four 0x190-byte pages. */
void reset_tables_45ba0(void)
{
    uint32_t offset;
    uint32_t page;

    for (offset = 0x14; offset != 0xc8; offset += 0x14u) {
        EF_U16(0x0012667cu + offset) = 0xff;
        EF_U16(0x0012667eu + offset) = 0xff;
    }
    EF_U16(0x0012667cu + 0xc8u) = 0xff;
    EF_U16(0x0012667eu + 0xc8u) = 0xff;
    for (page = 0; page < 4; page++) {
        uint32_t end = 0x118u + page * 0x190u;

        for (offset = page * 0x190u + 2u; ; offset += 2u) {
            EF_U16(0x001a4baeu + offset) = 0;
            if (offset == end)
                break;
        }
    }
    EF_U8(0x001a5228u) = 0;
}

/* 0x46128, 64 bytes. Retire the entries of the 9-slot table that belong to object. */
void retire_entries_46128(ef_addr32 object)
{
    uint32_t offset;

    if (object == 0)
        return;
    for (offset = 0; offset != 0xb4; offset += 0x14u) {
        if (EF_U16(0x00126692u + offset) == EF_U16(object + 0x16u)) {
            EF_U16(0x00126692u + offset) = 0xff;
            EF_U16(0x00126690u + offset) = 0x85;
        }
    }
}

/* 0x50770, 106 bytes. Largest (second - first) byte difference over the list at 0x137988. */
int32_t max_difference_50770(void)
{
    ef_addr32 node;
    int16_t best = 0;

    for (node = EF_U32(0x00137988u); node != 0; node = EF_U32(node + 4u)) {
        ef_addr32 entry;
        int16_t difference;

        if (EF_U8(node + 0xbu) != 0x31 || EF_U16(node + 0x36u) == 0)
            continue;
        if ((group_flag_positive_cc22c(0, EF_U8(node + 9u), 0x00137980u) & 0xffffu) == 0)
            continue;
        entry = guest_fn_bfcfc(0x00137958u, node);
        difference = (int16_t)((int32_t)EF_U8(EF_U32(entry + 0x33u)) - (int32_t)EF_U8(EF_U32(entry + 0x37u)));
        if (best < difference)
            best = difference;
    }
    return best;
}

/* 0x55784, 97 bytes. Release a unit's slot if it still refers to the unit's target. */
void release_slot_55784(ef_addr32 message)
{
    ef_addr32 unit = guest_fn_7489c(EF_S16(message + 3u));
    ef_addr32 units;
    ef_addr32 record;
    int32_t target;

    if (unit == 0)
        return;
    units = EF_U32(0x0019bb58u);
    record = units + (uint32_t)((int32_t)EF_S16(message + 1u) * 0xca) + (uint32_t)EF_U8(unit + 0x2au) * 2u;
    target = EF_S16(unit + 0x16u);
    if ((int32_t)EF_U16(record + 0x50u) != target)
        return;
    EF_U16(record + 0x50u) = 0xff;
    EF_U8(units + (uint32_t)((int32_t)EF_S16(message + 1u) * 0xca) + 0x4fu) -= 1u;
    guest_fn_2b708(EF_S16(message + 1u));
}

/* 0x55b80, 79 bytes. */
void apply_action_55b80(ef_addr32 message)
{
    ef_addr32 unit;
    ef_addr32 target;

    if (EF_S16(message + 1u) == 0xff)
        return;
    unit = guest_fn_7489c(EF_S16(message + 1u));
    if (unit == 0)
        return;
    target = guest_fn_87f9c(EF_U8(message + 8u), EF_S16(message + 6u));
    if (target == 0)
        return;
    guest_fn_8873c(EF_S16(message + 3u), unit, EF_U8(message + 5u), target);
}

/* 0x562d8, 61 bytes. Bump the per-slot counter of the first record whose key equals value. */
void bump_counter_562d8(uint32_t key, int16_t slot)
{
    int16_t i;
    uint8_t found = 0;

    for (i = 0; ; ) {
        ef_addr32 record = (uint32_t)((int32_t)i * 0x37);

        if (key == EF_U32(0x001a63f3u + record)) {
            found = 1;
            if (slot < 8)
                EF_U8(0x001a63c0u + record + (uint32_t)(int32_t)slot) += found;
        }
        i++;
        if (found != 0)
            break;
        if (i >= 10)
            break;
    }
}
