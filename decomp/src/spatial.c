/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"

/* 0x000263e0  size 0x0c  drafted: not compiled, spliced or guest-verified.
 * The original preserves the low half of the enclosing dword at 0x19b374;
 * only the word at 0x19b376 is reset. */
void reset_spatial_slot_count(void)
{
    g_spatial_slot_count = 0;
}


/* 0x000263ec  size 0x37  drafted: not compiled, spliced or guest-verified.
 * The source reads the count via an arithmetic shift of 0x19b374 by 16;
 * comparison against 29 is therefore signed. The original does not guard
 * negative counts; valid game states are expected to use 0..29. */
void append_spatial_slot(int16_t x, int16_t y, uint8_t tag)
{
    int16_t slot = g_spatial_slot_count;
    if (slot < 29) {
        g_spatial_slots[slot].x = x;
        g_spatial_slots[slot].y = y;
        g_spatial_slot_tags[slot] = tag;
        g_spatial_slot_count = (int16_t)(slot + 1);
    }
}
