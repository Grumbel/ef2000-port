/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Host-model test only: original-game equivalence still needs a guest diff. */
#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

int32_t g_hiscore_current;
ef_hiscore_entry g_hiscore_table[10];

int main(void)
{
    const struct {
        int32_t current;
        int32_t tenth;
        uint8_t qualifies;
    } cases[] = {
        {0, 0, 0},
        {101, 100, 1},
        {100, 101, 0},
        {-1, -1, 0},
        {-2, -3, 1},
        {-3, -2, 0},
        {INT32_MIN, INT32_MAX, 0},
        {INT32_MAX, INT32_MIN, 1},
        {-1, 0, 0},
        {0, -1, 1},
    };
    unsigned char saved_table[sizeof(g_hiscore_table)];
    size_t i;

    assert(sizeof(ef_hiscore_entry) == 25);
    assert(sizeof(g_hiscore_table) == 250);
    assert(offsetof(ef_hiscore_entry, score) == 21);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(g_hiscore_table, 0xa5, sizeof(g_hiscore_table));
        g_hiscore_current = cases[i].current;
        g_hiscore_table[9].score = cases[i].tenth;
        memcpy(saved_table, g_hiscore_table, sizeof(saved_table));
        assert(hiscore_qualifies() == cases[i].qualifies);
        assert(g_hiscore_current == cases[i].current);
        assert(memcmp(saved_table, g_hiscore_table, sizeof(saved_table)) == 0);
    }
    return 0;
}
