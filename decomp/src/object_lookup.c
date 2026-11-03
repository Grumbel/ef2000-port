/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"

/* 0x0007489c  noncontiguous 0x5d-byte body (0x58 here plus 0x05 at
 * 0x6a1ac). Draft: not compiled, spliced or guest-verified. Rejection
 * branches in the original share another function's zero-return epilogue;
 * a future in-place splice must account for that cross-function edge.
 * Preserve the diagnostic increment even for a rejected lookup. */
ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    ef_addr32 object;

    ++g_filtered_object_lookup_calls;
    if (index < 0 || index >= g_indexed_object_count ||
        index == 0xff || index == g_excluded_object_index) {
        return 0;
    }
    object = g_indexed_object_table[index];
    if (object == 0xff) {
        return 0;
    }
    return object;
}
