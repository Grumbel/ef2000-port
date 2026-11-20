/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* The runtime's environment table: g_environ is a null-terminated array
 * of "NAME=value" pointers (getenv at 0x9c858 reads it). Once putenv has
 * allocated it, g_environ_owned points to one flag byte per entry, kept
 * in the same block after the null pointer and one spare slot; a nonzero
 * flag means the entry string belongs to the heap. putenv itself always
 * clears the flag: it stores the caller's string, not a copy. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xf44b6, 224 bytes. Look up the name of ASSIGNMENT ("NAME=..."),
 * ignoring ASCII case and only up to the table entry's '='. Returns the
 * one-based index of the entry, or minus the entry count if there is
 * none. With REMOVE set, a found entry is deleted instead and 0 is
 * returned: the pointers move down over it, the flag bytes move to
 * start at the old null slot, and the deleted entry's string is freed
 * if it was owned. */
int32_t find_env_entry_f44b6(ef_addr32 assignment, int32_t remove)
{
    ef_addr32 slot;

    for (slot = g_environ; EF_U32(slot) != 0u; slot += 4u) {
        ef_addr32 entry = EF_U32(slot);
        ef_addr32 name = entry;
        ef_addr32 wanted = assignment;
        int32_t index;
        int32_t count;
        ef_addr32 flags;

        while (EF_U8(wanted) != 0u) {
            if (to_upper_ascii(EF_U8(name)) != to_upper_ascii(EF_U8(wanted)))
                break;
            if (EF_U8(name) == '=')
                goto found;
            ++name;
            ++wanted;
        }
        continue;

    found:
        index = (int32_t)(slot - g_environ) >> 2;
        if (remove == 0)
            return index + 1;
        for (; EF_U32(slot) != 0u; slot += 4u)
            EF_U32(slot) = EF_U32(slot + 4u);
        flags = g_environ_owned;
        if (flags == 0u)
            return 0;
        if (EF_U8(flags + (uint32_t)index) != 0u)
            heap_free_find_fa9d2(entry);
        count = (int32_t)(slot - g_environ) >> 2;
        guest_memmove_109e6f(slot, g_environ_owned, (uint32_t)count);
        g_environ_owned = slot;
        /* Moves the byte after the last flag down as well. */
        for (; index < count; ++index)
            EF_U8(slot + (uint32_t)index) = EF_U8(slot + (uint32_t)index + 1u);
        return 0;
    }
    return -(int32_t)((slot - g_environ) >> 2);
}

/* 0xf4347, 367 bytes. putenv: "NAME=value" sets NAME, "NAME=" removes
 * it. Returns 0, or -1 without an '=' or when the heap is exhausted.
 * A new entry grows the block by five bytes (pointer and flag); a block
 * whose flags were never set up is copied into a fresh one, otherwise it
 * is reallocated and the flags are moved from their old address. */
int32_t putenv_f4347(ef_addr32 assignment)
{
    ef_addr32 equal = assignment;
    ef_addr32 table;
    int32_t remove;
    int32_t count;

    if (assignment == 0u)
        return -1;
    while (EF_U8(equal) != 0u && EF_U8(equal) != '=')
        ++equal;
    if (EF_U8(equal) == 0u)
        return -1;
    remove = EF_U8(equal + 1u) == 0u;

    table = g_environ;
    if (table == 0u) {
        if (remove)
            return 0;
        table = allocate_heap_fc795(9u);
        if (table == 0u)
            return -1;
        g_environ = table;
        EF_U32(table) = 0u;
        EF_U32(table + 4u) = 0u;
        g_environ_owned = table + 8u;
        count = 0;
    } else {
        int32_t found = find_env_entry_f44b6(assignment, remove);
        uint32_t flags_offset;
        uint32_t size;

        if (remove)
            return 0;
        if (found > 0) {
            count = found - 1;
        } else {
            count = -found;
            flags_offset = (uint32_t)count * 4u + 8u;
            size = flags_offset + (uint32_t)count + 1u;
            if (g_environ_owned == 0u) {
                table = allocate_heap_fc795(size);
                if (table == 0u)
                    return -1;
                memcpy_dc832(table, g_environ, (uint32_t)count * 4u);
                g_environ_owned = table + flags_offset;
                fill_bytes_b3fb0(g_environ_owned, 0u, (uint32_t)count + 1u);
            } else {
                table = reallocate_heap_e8651(table, size);
                if (table == 0u)
                    return -1;
                guest_memmove_109e6f(table + flags_offset, g_environ_owned, (uint32_t)count);
                g_environ_owned = table + flags_offset;
            }
            g_environ = table;
            EF_U32(table + (uint32_t)count * 4u + 4u) = 0u;
        }
    }
    EF_U32(table + (uint32_t)count * 4u) = assignment;
    EF_U8(g_environ_owned + (uint32_t)count) = 0u;
    return 0;
}
