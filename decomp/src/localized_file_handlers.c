/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9cf2c, 72 bytes. Construct the selected-language candidate path.
 * FRAME replaces the original 0x90-byte Watcom stack local in its callers.
 * If NAME already starts with the default directory, do not duplicate it. */
void build_localized_path_9cf2c(ef_addr32 frame, ef_addr32 name)
{
    ef_addr32 suffix = name;
    uint32_t prefix_length = EF_U32(0x0040d1e0u);
    if (prefix_length != 0 &&
        compare_strings_e5c03(name, 0x0040d0c0u, prefix_length) == 0)
        suffix += prefix_length;
    copy_string_9d749(frame, 0x0040d150u);
    (void)strcat_9ca57(frame, suffix);
}

/* 0x9cf74, 48 bytes. Try the localized path, then the original filename. */
uint32_t size_localized_file_9cf74(ef_addr32 name, ef_addr32 frame)
{
    uint32_t size;
    build_localized_path_9cf2c(frame, name);
    size = find_in_archives_9cb20(frame);
    if (size == 0)
        size = find_in_archives_9cb20(name);
    return size;
}

/* 0x9cfa4, 56 bytes. Use the same resolution for the read callback.
 * The Watcom implementation preserves the caller's ECX (allocated size + 1).
 * The derived C text loader retains that count in its own local variable. */
uint32_t load_localized_file_9cfa4(ef_addr32 name, ef_addr32 dest,
                                  ef_addr32 frame)
{
    build_localized_path_9cf2c(frame, name);
    if (find_in_archives_9cb20(frame) != 0)
        return try_handlers_9cbc4(frame, dest);
    return try_handlers_9cbc4(name, dest);
}
