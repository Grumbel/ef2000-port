/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"

/* 0x0006a1b4  size 0x1a  drafted: not compiled, spliced or guest-verified.
 * The original compares signed AX against a signed 16-bit count, rejects
 * negative indices, and loads one 32-bit guest address. It does not deref
 * that address or test for a null table entry. */
ef_addr32 lookup_indexed_object(int16_t index)
{
    if (index >= 0 && index < g_indexed_object_count) {
        return g_indexed_object_table[index];
    }
    return 0;
}
