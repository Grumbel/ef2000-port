/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_structs/map_entry.h"

/* -200..199 from the low word of half_word_9c960. */
static int32_t random_jitter(void)
{
    return (int32_t)(uint16_t)half_word_9c960() % 400 - 200;
}

/* 0x90ac4, 357 bytes. In play mode (g_tactical_action_enabled) place
 * ID at (X, Z, Y) in the first map entry whose state bits 0 or 1 are
 * set: clear its words, clock and state bits, give it a random jitter,
 * its slot number, the grid marker and score 100, then run the page
 * text, page and entry opens for its kind, step the script store and,
 * in grid mode, send the entry (0x573d0). */
void place_map_entry_90ac4(int16_t id, int32_t x, int32_t z, int32_t y)
{
    int16_t slot;

    if (g_tactical_action_enabled == 0u)
        return;
    for (slot = 0; slot < EF_MAP_ENTRY_COUNT; ++slot) {
        ef_map_entry *e = (ef_map_entry *)(uintptr_t)(EF_MAP_ENTRY_TABLE + (uint32_t)slot * sizeof(ef_map_entry));

        if ((e->flags & 3u) == 0u)
            continue;
        e->x = x;
        e->z = z;
        e->y = y;
        e->words[0] = 0;
        e->words[1] = 0;
        e->words[2] = 0;
        e->flags &= 0xf8u;
        e->clock = 0u;
        e->jitter[0] = random_jitter();
        e->clock = 0u;
        e->jitter[1] = random_jitter();
        e->index = slot;
        e->id = id;
        e->marker = g_world_grid_marker;
        e->score = 100u;
        run_page_text_9e508(e->kind, 0u);
        select_page_9e3f8((uint32_t)(int32_t)e->kind);
        open_entry_cd150(e->kind);
        script_store_step_cd20c(0u);
        if (g_world_grid_mode != 0u)
            send_table_entry_573d0(slot, 1u);
        return;
    }
}
