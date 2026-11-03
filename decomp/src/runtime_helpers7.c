/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu-page and path-building library helpers, 0x9c9c0..0xa2634. */

/* Address of the 29-byte (0x1d) slot record for a page index in the table at 0x417174. */
static ef_addr32 page_slot(uint32_t index)
{
    return EF_U32(0x00417174u) + 0xbu + index * 29u;
}

/* 0x9c9c0, 86 bytes. Build a search path: default directory + separator + name. */
void build_path_9c9c0(ef_addr32 buffer, ef_addr32 name)
{
    ef_addr32 directory;

    EF_U8(buffer) = 0;
    if (EF_U32(0x00132aa8u) == 0)
        EF_U32(0x00132aa8u) = guest_fn_9c858(0x0018b154u);
    directory = EF_U32(0x00132aa8u);
    if (directory != 0) {
        copy_string_9d749(buffer, directory);
        guest_fn_9ca57(buffer, 0x0018b158u);
    }
    guest_fn_9ca57(buffer, name);
    guest_fn_eb478(buffer);
}

/* 0x9d7d4, 54 bytes. Reset the clip words; the first stores are overwritten in the original. */
void set_clip_9d7d4(int32_t x, int32_t y)
{
    EF_U16(0x00417224u) = (uint16_t)x;
    EF_U16(0x00417228u) = (uint16_t)y;
    EF_U16(0x00417222u) = 0x8000u;
    EF_U16(0x00417224u) = (uint16_t)(0u - (uint32_t)x);
    EF_U16(0x00417228u) = (uint16_t)(0u - (uint32_t)y);
}

/* 0x9dc88, 110 bytes. Lower-case the name part (at most 8 chars) of a file name and append it to a path. */
void append_base_name_9dc88(ef_addr32 scratch, ef_addr32 path, ef_addr32 name)
{
    int16_t i = 0;

    for (;;) {
        uint8_t c = EF_U8(name + (uint32_t)(int32_t)i);

        if (c == 0 || c == 0x2e || (int32_t)i == 8)
            break;
        if (c >= 0x41 && c <= 0x5a)
            c = (uint8_t)(c + 0x20);
        EF_U8(scratch + (uint32_t)(int32_t)i) = c;
        i++;
    }
    EF_U8(scratch + (uint32_t)(int32_t)i) = 0;
    copy_string_9d749(path, 0x0041719au);
    guest_fn_9ca57(path, scratch);
    guest_fn_9ca57(path, 0x0018b2a0u);
}

/* 0x9f7d0, 55 bytes. Clear the flag byte of a page slot and mark the page dirty. */
void clear_page_slot_9f7d0(int16_t index, uint32_t value)
{
    ef_addr32 slot = page_slot((uint32_t)(int32_t)index);

    EF_U8(slot + 0x1cu) = 0;
    store_table_entry_9dde0(slot, value & 0xffffu);
    EF_U8(EF_U32(0x00417174u) + 0xau) |= 0x80u;
}

/* 0x9f808, 69 bytes. Set flags and a relative word offset in a page slot and mark the page dirty. */
void set_page_slot_9f808(int16_t index, uint32_t value, uint8_t flags, uint32_t position)
{
    ef_addr32 slot = page_slot((uint32_t)(int32_t)index);

    EF_U8(slot + 0x1cu) = flags;
    EF_U16(slot + 0x12u) = (uint16_t)(((int32_t)(position - EF_U32(0x0041717cu))) >> 1);
    store_table_entry_9dde0(slot, value & 0xffffu);
    EF_U8(EF_U32(0x00417174u) + 0xau) |= 0x80u;
}

/* 0xa0d58, 80 bytes. Copy count words into the current page from source. */
void copy_page_words_a0d58(int16_t offset, uint16_t count, ef_addr32 source)
{
    ef_addr32 page = page_slot(EF_U8(EF_U32(0x0041717cu) + 1u));
    ef_addr32 destination = page + (uint32_t)((int32_t)offset * 2);
    uint16_t remaining = count;

    do {
        EF_U16(destination) = EF_U16(source);
        source += 2u;
        destination += 2u;
        remaining--;
    } while (remaining != 0);
    EF_U8(EF_U32(0x00417174u) + 0xau) |= 0x80u;
}

/* 0xa0018, 69 bytes. Find the entry in the 0x4e-byte chain whose two keys match, or -1. */
int32_t find_chain_entry_a0018(uint16_t key_high, uint16_t key_low)
{
    int32_t index = (int16_t)EF_U16(0x00132b26u);

    while (index != -1) {
        ef_addr32 entry = 0x00416e40u + (uint32_t)(index * 0x4e);

        if (EF_U16(entry + 2u) == key_high && EF_U16(entry) == key_low)
            return index;
        index = (int16_t)EF_U16(entry + 4u);
    }
    return index;
}

/* 0xa1a90, 83 bytes. Look up key in the (key, offset) pair table of a page; return the address or 0. */
uint32_t find_page_pair_a1a90(int16_t page, int32_t key)
{
    ef_addr32 record = guest_fn_b3f58((int32_t)page);
    ef_addr32 table = EF_U32(0x00414f00u + (uint32_t)EF_U16(record) * 4u);
    int16_t count;

    if (table == 0)
        return 0;
    table -= 2u;
    count = (int16_t)EF_U16(table + 2u);
    for (;;) {
        if ((int32_t)(int16_t)EF_U16(table - 2u) == key)
            return (uint32_t)((int32_t)(int16_t)EF_U16(table) + (int32_t)EF_U32(0x0040d200u + (uint32_t)EF_U16(record) * 4u));
        count--;
        table -= 4u;
        if (count <= 0)
            return 0;
    }
}

/* 0xa2634, 100 bytes. Fill one entry of the current page and refresh the page slot. */
void set_page_entry_a2634(int16_t index, int16_t entry, uint32_t value)
{
    ef_addr32 slot = page_slot((uint32_t)(int32_t)index);
    ef_addr32 page;
    ef_addr32 target;

    EF_U8(slot + 0x1cu) &= 0x3fu;
    page = page_slot(EF_U8(EF_U32(0x0041717cu) + 1u));
    target = page + (uint32_t)((int32_t)entry * 2);
    EF_U16(target + 4u) = 0x4000;
    EF_U32(target) = value;
    guest_fn_a207c(target, slot);
}
