/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint8_t *row_bytes(int16_t cell)
{
    return (uint8_t *)ef_guest_memory_ptr(g_world_grid_base) + (int32_t)cell * 0xca;
}

/* A row's attached-object count is the byte at +0x4f; each signed index
 * starts at +0x50, two bytes per entry. The first index is also the high
 * half of the 32-bit field at +0x4e used by other original routines. */
static int16_t attached_index(const uint8_t *row, uint16_t entry)
{
    uint32_t offset = 0x50U + (uint32_t)entry * 2U;
    uint16_t bits = (uint16_t)row[offset] |
                    (uint16_t)((uint16_t)row[offset + 1U] << 8);
    return (int16_t)bits;
}

/* 0x0002beb0  size 0x55  drafted. Never short-circuit the seven lookup
 * attempts: each visit increments the original diagnostic lookup counter. */
uint8_t grid_has_active_link(int16_t cell)
{
    uint16_t entry;
    uint8_t found = 0;
    for (entry = 0; entry < 7; ++entry) {
        const uint8_t *row = row_bytes(cell);
        int16_t index = attached_index(row, entry);
        ef_addr32 ptr = lookup_filtered_indexed_object(index);
        if (ptr != 0) {
            const uint8_t *object = (const uint8_t *)ef_guest_memory_ptr(ptr);
            if (object[0x18d] != 0) {
                found = 1;
            }
        }
    }
    return found;
}

/* 0x0002bf08  size 0x5f  drafted. The +0x4f entry count is not capped at
 * seven in the original; the iteration also continues after a match. */
uint8_t grid_has_object_mode(int16_t cell, uint8_t mode)
{
    uint16_t entry = 0;
    uint8_t found = 0;
    for (;;) {
        const uint8_t *row = row_bytes(cell);
        uint8_t count = row[0x4f];
        if (entry >= count) {
            break;
        }
        {
            int16_t index = attached_index(row, entry);
            ef_addr32 ptr = lookup_filtered_indexed_object(index);
            if (ptr != 0) {
                const uint8_t *object = (const uint8_t *)ef_guest_memory_ptr(ptr);
                if ((object[0x24] & 0x08) == 0 && object[0x16c] == mode) {
                    found = 1;
                }
            }
        }
        ++entry;
    }
    return found;
}

/* 0x0002dba4  size 0x52  drafted. The 0xff sentinel is a signed-AX
 * equality check; other negative values remain eligible for the loop. */
uint8_t grid_is_current_or_external(int16_t cell)
{
    uint16_t i;
    if (cell == 0xff) {
        return 0;
    }
    if (cell == g_active_grid_index) {
        return 1;
    }
    if (g_world_grid_mode != 0) {
        for (i = 0; i < g_external_grid_count; ++i) {
            if (external_grid_link_matches(cell, (uint8_t)i) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* 0x0002e910  size 0x39  drafted. Byte-level event relation predicate;
 * this changes AH and sometimes EDX in the original, but callers only
 * consume AL. No input signedness applies to either byte. */
uint8_t grid_event_matches(uint8_t code, uint8_t mode)
{
    if (code == 0) return (uint8_t)(mode == 1);
    if (code == 1) return (uint8_t)(mode == 0);
    if (code == 2) return (uint8_t)(mode != 2);
    return 0;
}
