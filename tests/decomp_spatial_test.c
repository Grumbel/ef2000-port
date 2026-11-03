/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Host model only. Full guest differential and register checks are pending. */
#include "ef_globals.h"
#include "ef_abi.h"

#include <assert.h>
#include <string.h>

int16_t g_spatial_slot_count;
ef_spatial_slot g_spatial_slots[29];
uint8_t g_spatial_slot_tags[29];

int main(void)
{
    static const uint16_t initial_counts[] = {0, 1, 0x7fff, 0x8000, 0xffff};
    unsigned i;
    for (i = 0; i < sizeof(initial_counts) / sizeof(initial_counts[0]); ++i) {
        g_spatial_slot_count = (int16_t)initial_counts[i];
        reset_spatial_slot_count();
        assert(g_spatial_slot_count == 0);
    }

    memset(g_spatial_slots, 0xa5, sizeof(g_spatial_slots));
    memset(g_spatial_slot_tags, 0x5a, sizeof(g_spatial_slot_tags));
    for (i = 0; i < 29; ++i) {
        append_spatial_slot((int16_t)(0x100 + i), (int16_t)(-0x100 - (int)i),
                            (uint8_t)(0x40 + i));
        assert(g_spatial_slot_count == (int16_t)(i + 1));
        assert(g_spatial_slots[i].x == (int16_t)(0x100 + i));
        assert(g_spatial_slots[i].y == (int16_t)(-0x100 - (int)i));
        assert(g_spatial_slot_tags[i] == (uint8_t)(0x40 + i));
        if (i < 28) {
            assert(g_spatial_slot_tags[i + 1] == 0x5a);
        }
    }
    {
        ef_spatial_slot saved_slots[29];
        uint8_t saved_tags[29];
        memcpy(saved_slots, g_spatial_slots, sizeof(saved_slots));
        memcpy(saved_tags, g_spatial_slot_tags, sizeof(saved_tags));
        append_spatial_slot(1000, 2000, 0xff);
        assert(g_spatial_slot_count == 29);
        assert(memcmp(g_spatial_slots, saved_slots, sizeof(saved_slots)) == 0);
        assert(memcmp(g_spatial_slot_tags, saved_tags, sizeof(saved_tags)) == 0);
        g_spatial_slot_count = 30;
        append_spatial_slot(1000, 2000, 0xff);
        assert(g_spatial_slot_count == 30);
        assert(memcmp(g_spatial_slots, saved_slots, sizeof(saved_slots)) == 0);
        assert(memcmp(g_spatial_slot_tags, saved_tags, sizeof(saved_tags)) == 0);
    }
    reset_spatial_slot_count();
    assert(g_spatial_slot_count == 0);
    append_spatial_slot(-32768, 32767, 0xff);
    assert(g_spatial_slots[0].x == -32768);
    assert(g_spatial_slots[0].y == 32767);
    assert(g_spatial_slot_tags[0] == 0xff);
    assert(g_spatial_slot_count == 1);
    return 0;
}
