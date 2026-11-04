/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Page table, string compare and table search helpers, 0xe3c6c..0xeb4dc. */

/* 0xe3c6c, 61 bytes. Resolve a page slot from the 0x190a44 chain and store it in 0x159134. */
void select_page_e3c6c(uint32_t stack_page)
{
    int16_t page = (int16_t)stack_page;
    ef_addr32 key;
    ef_addr32 pair;

    if (page == -1)
        return;
    key = guest_fn_a1a40_key(0x00190a44u);
    if (key == 0)
        return;
    pair = find_page_pair_a1a90(page, (int32_t)key);
    if (pair == 0)
        return;
    EF_U32(0x00159134u) = select_page_slot_a0de0(page, (int16_t)EF_U16(pair));
}

/* 0xe6120, 35 bytes. Store five scalar settings. */
void set_settings_e6120(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t stack_e)
{
    EF_U32(0x00433ed4u) = a;
    EF_U32(0x00433ed8u) = b;
    EF_U32(0x00433ee0u) = c;
    EF_U32(0x00433edcu) = d;
    EF_U32(0x00433ed0u) = stack_e;
}

/* 0xe7c50, 63 bytes. Store a word into the 0x190c3c page table entry. */
void set_page_word_e7c50(int16_t page, uint16_t value, int16_t index)
{
    ef_addr32 key = guest_fn_a1a40_key(0x00190c3cu);
    ef_addr32 pair;

    if (key == 0)
        return;
    pair = find_page_pair_a1a90(page, (int32_t)key);
    if (pair == 0)
        return;
    EF_U16(select_page_slot_a0de0(page, (int16_t)EF_U16(pair)) + (uint32_t)((int32_t)index * 2)) = value;
}

/* 0xe7c90, 64 bytes. Read a word from the 0x190c3c page table entry, or 0. */
uint16_t get_page_word_e7c90(int16_t page, int16_t index)
{
    ef_addr32 key = guest_fn_a1a40_key(0x00190c3cu);
    ef_addr32 pair;

    if (key == 0)
        return 0;
    pair = find_page_pair_a1a90(page, (int32_t)key);
    if (pair == 0)
        return 0;
    return EF_U16(select_page_slot_a0de0(page, (int16_t)EF_U16(pair)) + (uint32_t)((int32_t)index * 2));
}

/* 0xeb374, 87 bytes. Case-insensitive compare of up to n bytes. */
int32_t compare_nocase_eb374(ef_addr32 a, ef_addr32 b, int32_t count)
{
    while (count != 0) {
        uint8_t ca = EF_U8(a);
        uint8_t cb = EF_U8(b);

        if (ca >= 0x41 && ca <= 0x5a)
            ca = (uint8_t)(ca + 0x20);
        if (cb >= 0x41 && cb <= 0x5a)
            cb = (uint8_t)(cb + 0x20);
        if (ca != cb)
            return (int32_t)ca - (int32_t)cb;
        if (cb == 0)
            return 0;
        a++;
        b++;
        count--;
    }
    return 0;
}

/* 0xeb4dc, 65 bytes. Binary search 12-byte entries sorted by their first signed dword. */
ef_addr32 bsearch_entries_eb4dc(ef_addr32 key_ptr, ef_addr32 table, int32_t count)
{
    int32_t key = EF_S32(key_ptr);
    int32_t low = 0;
    int32_t high = count - 1;

    while (low <= high) {
        int32_t mid = (low + high) >> 1;
        ef_addr32 entry = table + (uint32_t)(mid * 12);
        int32_t value = EF_S32(entry);

        if (value == key)
            return entry;
        if (value > key) {
            if (mid == 0)
                return 0;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return 0;
}
