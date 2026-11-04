/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* List search and counting helpers, 0xb4cdc..0xbecc0. */

/* 0xb4cdc, 91 bytes. As find_child_b4c20 with the 0xcc22c check. */
ef_addr32 find_child_b4cdc(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context)
{
    ef_addr32 parent = EF_U32(list);

    while (parent != 0) {
        if (EF_U16(parent + 9u) == EF_U16(key + 9u)) {
            for (;;) {
                ef_addr32 child = EF_U32(parent);

                if (child == 0)
                    return 0;
                if (EF_U8(child + 0xcu) == 0x31
                    && (guest_fn_cc22c(kind, EF_U8(child + 0xbu), context) & 0xffffu) != 0)
                    return child;
                parent = EF_U32(parent);
            }
        }
        parent = EF_U32(parent + 4u);
    }
    return 0;
}

/* 0xb4d38, 93 bytes. As find_sibling_b4c7c with the 0xcc22c check. */
ef_addr32 find_sibling_b4d38(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context)
{
    ef_addr32 parent = EF_U32(list);

    while (parent != 0) {
        if (EF_U16(parent + 9u) == EF_U16(key + 9u)) {
            for (;;) {
                ef_addr32 child = EF_U32(parent + 4u);

                if (child == 0)
                    return 0;
                if (EF_U8(child + 0xcu) == 0x31
                    && (guest_fn_cc22c(kind, EF_U8(child + 0xbu), context) & 0xffffu) != 0)
                    return child;
                parent = EF_U32(parent + 4u);
            }
        }
        parent = EF_U32(parent + 4u);
    }
    return 0;
}

/* 0xbd73c, 94 bytes. True when some window of (width) consecutive 9-byte entries are all unused. */
uint32_t has_free_run_bd73c(ef_addr32 object, int16_t width)
{
    int16_t start;

    for (start = 0; (int32_t)start < (int32_t)((int32_t)EF_U32(object + 0x46u) >> 16) - (int32_t)width * 2; start++) {
        int16_t index = start;
        int16_t free_count = 0;

        while ((int32_t)index < (int32_t)start + (int32_t)width * 2) {
            ef_addr32 entry = EF_U32(object + 0x4au) + (uint32_t)((int32_t)index * 9);

            if (EF_U8(entry + 3u) == 0)
                free_count++;
            index++;
        }
        if (free_count >= width)
            return 1;
    }
    return 0;
}

/* 0xbd86c, 92 bytes. True when a non-kind-2 record has a filled entry (scanned from the end) that passes the check. */
uint32_t record_has_valid_entry_bd86c(ef_addr32 record)
{
    int16_t found = 0;
    int16_t i;

    if (EF_U8(record + 1u) == 2)
        return 0;
    for (i = (int16_t)(EF_U8(record + 5u) - 1); i >= 0 && found == 0; i--) {
        ef_addr32 entry = EF_U32(record + 6u) + (uint32_t)((int32_t)i * 10);

        if (EF_U16(entry) != 0 && (guest_fn_bd7cc(entry, EF_U8(record)) & 0xffffu) != 0)
            found = 1;
    }
    return (uint32_t)(uint16_t)found;
}

/* 0xbd8c8, 100 bytes. True when the list's index-th node matching the criteria exists. */
uint32_t nth_match_exists_bd8c8(uint8_t kind, int16_t index, uint8_t sub, ef_addr32 target, ef_addr32 stack_list)
{
    ef_addr32 node = EF_U32(stack_list);
    uint16_t seen = 0;

    while (node != 0) {
        if (EF_U8(node + 0xcu) == kind && EF_U8(node + 0xdu) == 0 && EF_U16(node + 9u) == 0
            && EF_U8(node + 0xeu) == sub && EF_U32(node + 0x3bu) == EF_U32(target + 0x11u)
            && EF_U32(node + 0x3fu) == EF_U32(target + 0x15u))
            seen++;
        if (seen == (uint16_t)index)
            return 1;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xbecc0, 55 bytes. Count the 21-byte entries of the object that pass the check. */
uint32_t count_valid_entries_becc0(ef_addr32 object)
{
    uint32_t count = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U16(object + 0x2au); i++) {
        if ((is_active_bec84(EF_U32(object + 0x2cu) + (uint32_t)((int32_t)i * 0x15)) & 0xffffu) != 0)
            count++;
    }
    return count;
}
