/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Host model only. Full guest differential and register checks are pending. */
#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <limits.h>

int16_t g_indexed_object_count;
ef_addr32 g_indexed_object_table[4];

int main(void)
{
    g_indexed_object_table[0] = 0x12345678;
    g_indexed_object_table[1] = 0;
    g_indexed_object_table[2] = 0xfedcba98;
    g_indexed_object_table[3] = 0xaabbccdd;

    g_indexed_object_count = 3;
    assert(lookup_indexed_object(0) == 0x12345678);
    assert(lookup_indexed_object(1) == 0);
    assert(lookup_indexed_object(2) == 0xfedcba98);
    assert(lookup_indexed_object(3) == 0);
    assert(lookup_indexed_object(-1) == 0);
    assert(lookup_indexed_object(INT16_MIN) == 0);
    assert(lookup_indexed_object(INT16_MAX) == 0);

    g_indexed_object_count = 0;
    assert(lookup_indexed_object(0) == 0);
    g_indexed_object_count = -1;
    assert(lookup_indexed_object(0) == 0);
    g_indexed_object_count = 1;
    assert(lookup_indexed_object(0) == 0x12345678);
    assert(lookup_indexed_object(1) == 0);
    return 0;
}
