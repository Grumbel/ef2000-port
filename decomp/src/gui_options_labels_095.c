/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_options_095.h"
#include "ef_mem.h"

#include <stdint.h>
#include <string.h>

enum { TABLE_PTR = 0x160a18, FLAGS_PTR = 0x160a1c };

/* The original case fold at ddbc8 maps only ASCII 'a'..'z' to uppercase.
 * Matching deliberately ends at '=' and ignores each assignment's value. */
static uint8_t upper(uint8_t ch)
{
    if (ch >= 'a' && ch <= 'z')
        return (uint8_t)(ch - ('a' - 'A'));
    return ch;
}

static int key_matches(ef_addr32 key, ef_addr32 assignment)
{
    for (;;) {
        uint8_t a = EF_U8(key++);
        uint8_t b = EF_U8(assignment++);
        if (upper(a) != upper(b))
            return 0;
        if (a == '=')
            return 1;
        if (a == 0u)
            return 0;
    }
}

/* 0x000f44b6, 224 bytes. Positive results are one-based slot indices;
 * a missing label returns minus the active entry count. With 'remove' set,
 * an existing entry is deleted and zero is returned. The pointer table and
 * ownership-flag bytes share one allocation but grow from opposite ends. */
int32_t gui_options_find_f44b6(ef_addr32 assignment, int32_t remove)
{
    ef_addr32 table = EF_U32(TABLE_PTR);
    uint32_t index;
    for (index = 0u;; ++index) {
        ef_addr32 entry = EF_U32(table + 4u * index);
        if (entry == 0u)
            return -(int32_t)index;
        if (!key_matches(entry, assignment))
            continue;
        if (remove == 0)
            return (int32_t)(index + 1u);

        {
            uint32_t count = index;
            ef_addr32 flags = EF_U32(FLAGS_PTR);
            ef_addr32 new_flags;
            while (EF_U32(table + 4u * count) != 0u)
                ++count;
            /* The original shifts the terminating null pointer too. */
            memmove(ef_guest_memory_ptr(table + 4u * index),
                    ef_guest_memory_ptr(table + 4u * (index + 1u)),
                    (count - index) * 4u);
            if (flags == 0u)
                return 0;
            if (EF_U8(flags + index) != 0u)
                gui_options_release_095(entry);
            /* ECX advances one past the shifted null slot before the
             * original memmove: the new flag area begins at the OLD null
             * pointer, leaving a four-byte gap after the new null slot. */
            new_flags = table + count * 4u;
            memmove(ef_guest_memory_ptr(new_flags),
                    ef_guest_memory_ptr(flags), count);
            EF_U32(FLAGS_PTR) = new_flags;

            /* ORIGINAL BUG: copies the byte after the final flag as well;
             * it does not explicitly zero the last ownership byte. */
            for (; index < count; ++index)
                EF_U8(new_flags + index) = EF_U8(new_flags + index + 1u);
        }
        return 0;
    }
}

/* 0x000f4347, 367 bytes. The first '=' selects the label/value boundary.
 * An empty value asks f44b6 to delete, while a nonempty one replaces or
 * appends the original pointer (the value string is not copied). */
int32_t gui_options_store_f4347(ef_addr32 assignment)
{
    ef_addr32 table, flags, equal;
    uint32_t count;
    int32_t slot;
    int remove;

    if (assignment == 0u)
        return -1;
    equal = assignment;
    while (EF_U8(equal) != 0u && EF_U8(equal) != '=')
        ++equal;
    if (EF_U8(equal) == 0u)
        return -1;
    remove = EF_U8(equal + 1u) == 0u;
    table = EF_U32(TABLE_PTR);
    if (table == 0u) {
        if (remove)
            return 0;
        table = gui_options_allocate_095(0xfc795u, 0u, 9u);
        if (table == 0u)
            return -1;
        EF_U32(TABLE_PTR) = table;
        EF_U32(table) = 0u;
        EF_U32(table + 4u) = 0u;
        flags = table + 8u;
        EF_U32(FLAGS_PTR) = flags;
        count = 0u;
    } else {
        slot = gui_options_find_f44b6(assignment, remove);
        if (remove)
            return 0;
        if (slot > 0) {
            count = (uint32_t)(slot - 1);
        } else {
            ef_addr32 new_table;
            ef_addr32 new_flags;
            count = (uint32_t)-slot;
            flags = EF_U32(FLAGS_PTR);
            if (flags == 0u) {
                new_table = gui_options_allocate_095(0xfc795u, 0u, count * 5u + 9u);
                if (new_table == 0u)
                    return -1;
                memcpy(ef_guest_memory_ptr(new_table), ef_guest_memory_ptr(table), count * 4u);
                new_flags = new_table + count * 4u + 8u;
                memset(ef_guest_memory_ptr(new_flags), 0, count + 1u);
            } else {
                new_table = gui_options_allocate_095(0xe8651u, table, count * 5u + 9u);
                if (new_table == 0u)
                    return -1;
                new_flags = new_table + count * 4u + 8u;
                /* A moving original realloc can invalidate 'flags': the
                 * guest allocator ABI/lifetime is not yet verified. */
                memmove(ef_guest_memory_ptr(new_flags),
                        ef_guest_memory_ptr(flags), count);
            }
            table = new_table;
            EF_U32(TABLE_PTR) = table;
            EF_U32(FLAGS_PTR) = new_flags;
            EF_U32(table + (count + 1u) * 4u) = 0u;
        }
    }
    EF_U32(table + count * 4u) = assignment;
    EF_U8(EF_U32(FLAGS_PTR) + count) = 0u;
    return 0;
}
