/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x40820, 0x4e bytes. The clock and argument are three signed little-endian
 * words. The original compares the most significant word first and writes
 * only AL, leaving the higher 24 EAX bits intact. */
uint8_t reference_clock_reaches(ef_addr32 guest_clock)
{
    const uint8_t *value = (const uint8_t *)ef_guest_memory_ptr(guest_clock);
    unsigned i;
    for (i = 0; i < 3; ++i) {
        int16_t left = (int16_t)((uint16_t)g_reference_clock[2 * i] |
                                 ((uint16_t)g_reference_clock[2 * i + 1] << 8));
        int16_t right = (int16_t)((uint16_t)value[2 * i] |
                                  ((uint16_t)value[2 * i + 1] << 8));
        if (left > right) { return 1; }
        if (left < right) { return 0; }
    }
    return 1;
}

/* 0x41da4, 0x4f bytes. Thirteen consecutive command codes, full EAX result. */
uint32_t command_code_is_group_1b(uint8_t code)
{
    return code >= 0x1bu && code <= 0x27u ? 1u : 0u;
}

/* 0x49054, 0x3d bytes. Zero only these known fields, not surrounding bytes. */
void clear_tactical_accumulators(void)
{
    g_tactical_accumulators[0] = 0;
    g_tactical_accumulators[1] = 0;
    g_tactical_accumulators[2] = 0;
    g_tactical_status_words[0] = 0;
    g_tactical_status_words[1] = 0;
    g_tactical_status_byte = 0;
    g_tactical_runtime_count = 0;
}

/* 0x4d8f4, 0x2a bytes. The original stores indices 0..15 (not 1..16). */
void reset_tactical_slots(void)
{
    unsigned i;
    for (i = 0; i < 16; ++i) { g_tactical_slots[i] = 0xff; }
    if (g_tactical_reset_mode == 0) { g_tactical_low_flag = 0; }
}

/* 0x4dc98, 0x3d bytes. Starts at index one and changes only the first 8. */
void advance_first_tactical_status(void)
{
    unsigned i;
    for (i = 1; i < 25; ++i) {
        if (g_tactical_status_entries[i] == 8) {
            g_tactical_status_entries[i] = 0x12;
            g_tactical_status_changed = 0;
            return;
        }
    }
}

/* 0x544c0, 0x35 bytes. AH is an 8-bit counter; AL scans byte indices. */
void count_external_grid_markers(void)
{
    uint8_t total = 0;
    uint8_t i;
    for (i = 0; i < g_external_grid_count; ++i) {
        if (g_grid_external_probes[i].unknown_04 != 0) { ++total; }
    }
    g_external_grid_marked_count = total;
}

/* 0x5cb9c, 0x2a bytes. Inclusive, signed 16-bit XY bounds. AL-only return. */
uint8_t position_within_tactical_bounds(int16_t x, int16_t y)
{
    return x >= g_tactical_min_x && x <= g_tactical_max_x &&
           y >= g_tactical_min_y && y <= g_tactical_max_y;
}

/* 0x5f1ec, 0x34 bytes. Preserves both 32-bit overflow and the +0x400 bias. */
void sample_active_coordinates(ef_addr32 destination)
{
    uint32_t *out = (uint32_t *)ef_guest_memory_ptr(destination);
    if (g_active_coordinate_source == 0) {
        out[0] = 0;
        out[1] = 0;
    } else {
        const uint8_t *source = (const uint8_t *)ef_guest_memory_ptr(g_active_coordinate_source);
        uint32_t a, b;
        /* Packed guest data: avoid potentially unaligned 32-bit host reads. */
        a = (uint32_t)source[8] | ((uint32_t)source[9] << 8) |
            ((uint32_t)source[10] << 16) | ((uint32_t)source[11] << 24);
        b = (uint32_t)source[12] | ((uint32_t)source[13] << 8) |
            ((uint32_t)source[14] << 16) | ((uint32_t)source[15] << 24);
        out[0] = (a << 11) + 0x400u;
        out[1] = (b << 11) + 0x400u;
    }
}
