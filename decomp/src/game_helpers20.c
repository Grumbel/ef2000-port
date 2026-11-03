/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table lookups and vector helpers, 0x81354..0x887d0. */

/* 0x81354, 63 bytes. Find the 8-byte table entry whose name equals key; return its payload address or 0. */
ef_addr32 find_named_entry_81354(ef_addr32 key)
{
    uint32_t offset;

    for (offset = 0; (int32_t)offset < 0x80; offset += 8) {
        if (compare_strings_e5c03(EF_U32(0x0012b1a8u + offset), key, 2) == 0)
            return 0x0012b1a8u + offset + 4u;
    }
    return 0;
}

/* 0x821f8, 70 bytes. Index of the last 16-bit slot of row b (17 bytes per row) equal to a. */
uint8_t find_row_slot_821f8(uint16_t value, int16_t row)
{
    ef_addr32 table = EF_U32(0x001fcb40u);
    uint8_t found = 0; /* ORIGINAL BUG: the original returns an uninitialised byte when nothing matches. */
    uint8_t i;

    for (i = 0; i < 8; i++) {
        if (value == EF_U16(table + (uint32_t)((int32_t)row * 0x11) + (uint32_t)i * 2u + 1u))
            found = i;
    }
    EF_U32(0x001fcb40u) = table;
    return found;
}

/* 0x831a4, 89 bytes. Store a vector, halved until every component fits in 16 bits. */
void store_scaled_vector_831a4(ef_addr32 out, int32_t x, int32_t y, int32_t z)
{
    uint32_t mask = (uint32_t)((x >> 15) ^ (x >> 31)) | (uint32_t)((y >> 15) ^ (y >> 31)) | (uint32_t)((z >> 15) ^ (z >> 31));

    if (mask != 0) {
        do {
            x >>= 1;
            y >>= 1;
            mask >>= 1;
            z >>= 1;
        } while (mask != 0);
    }
    EF_U32(out) = (uint32_t)x;
    EF_U32(out + 4u) = (uint32_t)y;
    EF_U32(out + 8u) = (uint32_t)z;
}

/* 0x85c90, 104 bytes. Address of row `index` (6 bytes) in the selected unit's page table, or the default table. */
uint32_t page_row_85c90(int16_t index)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    uint32_t base = 0;
    uint32_t page;

    if (unit == 0)
        return 0x0012d794u;
    page = guest_fn_a1a40_key(0x0018a3b0u);
    if (page == 0)
        return 0x0012d794u;
    if (EF_U16(unit + 0x20u) != 0 && EF_U16(unit + 0x20u) != 0xffffu)
        base = find_page_pair_a1a90((int16_t)EF_U16(unit + 0x20u), (int32_t)page);
    return (uint32_t)((int32_t)index * 6) + base;
}

/* 0x887d0, 107 bytes. Flag the first kind-0x1f/0x20 record of the ring at 0x20f160 that passes the hit test. */
void flag_first_hit_887d0(void)
{
    ef_addr32 stop = EF_U32(0x0020f164u);
    ef_addr32 record = EF_U32(EF_U32(0x0020f160u) + 0x34u);

    while (record != stop) {
        uint8_t kind = EF_U8(record + 0x18u);

        if (kind == 0x20 || kind == 0x1f) {
            if (guest_fn_d0470((int16_t)EF_U16(record + 0x2eu), (int32_t)EF_U32(record) >> 8,
                               (int32_t)EF_U32(record + 4u) >> 8, (int32_t)EF_U32(record + 8u) >> 8) != 0) {
                EF_U8(record + 0x42u) = 1;
                guest_fn_cd2e8(0x64);
                return;
            }
        }
        stop = EF_U32(0x0020f164u);
        record = EF_U32(record + 0x34u);
    }
}
