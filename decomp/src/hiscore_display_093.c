/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_hiscore_093.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <stdint.h>
#include <stdio.h>

/* 0x40dc size 288. Three independent GUI icon list walks; every icon's
 * next pointer is unaligned at +0x21 and each column starts at its own head.
 * The adapters collapse the original dialog lookup and update calls but
 * preserve their order and the ten-row traversal cap. */
void hiscore_refresh_display_40dc(uint32_t force)
{
    static const char *const levels[] = {"Rookie", "Profi", "Topgun"};
    unsigned column;
    if (force == 0u && !hiscore_window_exists_093())
        return;
    for (column = 0u; column < 3u; ++column) {
        ef_addr32 icon = hiscore_first_icon_093(column);
        unsigned row;
        for (row = 0u; row < 10u && icon != 0u; ++row) {
            const ef_hiscore_entry *entry = &g_hiscore_table[row];
            if (column == 0u) {
                hiscore_icon_text_093(icon, entry->name);
            } else if (column == 1u) {
                if (entry->difficulty >= 1u && entry->difficulty <= 3u)
                    hiscore_icon_text_093(icon, levels[entry->difficulty - 1u]);
                else
                    hiscore_invalid_level_093(0x17f664u);
            } else {
                char number[32];
                (void)snprintf(number, sizeof(number), "p%ld", (long)entry->score);
                hiscore_icon_text_093(icon, number);
            }
            icon = EF_U32(icon + 0x21u);
        }
    }
}
