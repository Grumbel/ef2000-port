/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static ef_world_grid_cell *grid_row(int16_t cell)
{
    return (ef_world_grid_cell *)((uint8_t *)ef_guest_memory_ptr(g_world_grid_base)
                                  + (int32_t)cell * 0xca);
}

static uint8_t category_priority(uint8_t type)
{
    if (type == 6 || type == 7 || type == 8 || type == 14 || type == 28)
        return 1;
    if (type == 12 || type == 13) return 2;
    if (type == 9 || type == 15) return 3;
    return 0xff;
}

/* 0x0002da54  size 0x10d  drafted. Callers 0x161d4, 0x16588, 0x2d894.
 * Two possible object indices occupy +0x50/+0x52 in the grid row.
 * The original selects the +0x52 index only when the low unsigned word
 * of +0x50 equals the high signed word of g_grid_secondary_selector.
 * Object feature entries are three bytes and begin at +0x19b, with a
 * following two-byte nonzero gate checked at +0x19c. */
void refresh_grid_category(int16_t cell)
{
    ef_world_grid_cell *row;
    int16_t object_index;
    ef_addr32 object;
    uint8_t selected_priority = 0xff;
    uint8_t selected_type = 0;
    uint16_t i;

    if (cell == 0xff) return;
    row = grid_row(cell);
    {
        const uint8_t *p = (const uint8_t *)row;
        uint16_t index_at_50 = (uint16_t)p[0x50] | ((uint16_t)p[0x51] << 8);
        uint16_t offset = (int32_t)index_at_50 ==
                          (int32_t)(int16_t)(g_grid_secondary_selector >> 16) ?
                          0x52 : 0x50;
        object_index = (int16_t)((uint16_t)p[offset] | ((uint16_t)p[offset + 1] << 8));
    }
    object = lookup_filtered_indexed_object(object_index);
    if (object == 0) return;

    {
        const uint8_t *bytes = (const uint8_t *)ef_guest_memory_ptr(object);
        for (i = 0; i < 20; ++i) {
            uint16_t offset = (uint16_t)(0x19b + i * 3);
            uint8_t type = bytes[offset];
            uint16_t present = (uint16_t)bytes[offset + 1] |
                               ((uint16_t)bytes[offset + 2] << 8);
            if (present != 0 &&
                (g_grid_category_types[(int8_t)type].allowed_kind == 2 || type == 1)) {
                uint8_t priority = category_priority(type);
                if (priority < selected_priority) {
                    selected_priority = priority;
                    selected_type = type;
                }
            }
        }
    }
    /* Reload the base after the lookup and scan; C original does so. */
    grid_row(cell)->category_code = selected_type;
}

/* 0x0002dce0  size 0x78  drafted. Callers 0x2e740 and 0x7e8c0.
 * The delegated distance predicates at 0x2dc84 / 0x2dbf8 are not yet
 * independently ABI-confirmed. The external scan never short-circuits. */
uint8_t grid_is_visible_from_reference(int16_t cell)
{
    uint8_t result;
    uint16_t i;
    if (cell == 0xff) return 0;

    result = (uint8_t)(grid_reference_distance_matches(cell) != 0);
    if (g_world_grid_mode != 0) {
        for (i = 0; i < g_external_grid_count; ++i) {
            if (grid_external_distance_matches(cell, (uint8_t)i) != 0) result = 1;
        }
    }
    if (grid_row(cell)->current_mode == 0) result = 0;
    return result;
}
