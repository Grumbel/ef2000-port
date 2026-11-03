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

/* Object +0x177 is an unaligned IEEE-754 double. Access its representation
 * bytewise so this source does not require a runtime memcpy helper. */
static double read_object_limit(ef_addr32 object_ptr)
{
    const uint8_t *object = (const uint8_t *)ef_guest_memory_ptr(object_ptr);
    union { uint8_t bytes[8]; double value; } number;
    uint16_t i;
    for (i = 0; i < 8; ++i) number.bytes[i] = object[0x177 + i];
    return number.value;
}

static int16_t attached_index(const uint8_t *row, uint16_t entry)
{
    uint16_t offset = (uint16_t)(0x50 + entry * 2);
    return (int16_t)((uint16_t)row[offset] |
                     (uint16_t)((uint16_t)row[offset + 1] << 8));
}

/* 0x0002e08c  size 0xc9  drafted. Original callers 0x2ce18,
 * 0x2e030 and 0x85420. The x87 computes a rounded initial double limit,
 * then updates it from all seven indexed objects; NaNs are not discarded.
 * Exact x87 exception and extended-precision behavior awaits differential. */
void recompute_grid_limit(int16_t cell)
{
    uint16_t entry;
    ef_world_grid_cell *row;
    uint8_t type;
    double threshold;
    union { double value; uint32_t words[2]; } representation;

    if (cell == 0xff) return;
    row = grid_row(cell);
    type = row->object_type;
    {
        const ef_grid_type_record *record = &g_grid_type_records[type];
        long double extended = (long double)record->limit_a +
                               (long double)record->limit_b;
        extended += (long double)record->limit_multiplier *
                    (long double)g_grid_limit_scale;
        threshold = (double)extended;
    }
    for (entry = 0; entry < 7; ++entry) {
        /* The original reloads the row base for every slot. This matters
         * if filtered lookups have stateful side effects. */
        const uint8_t *bytes = (const uint8_t *)grid_row(cell);
        ef_addr32 object = lookup_filtered_indexed_object(attached_index(bytes, entry));
        if (object != 0) {
            double limit = read_object_limit(object);
            if (!(limit >= threshold)) {
                threshold = limit;
            }
        }
    }
    /* The original reloads the grid base again after the lookups. */
    row = grid_row(cell);
    representation.value = threshold;
    row->pending_a = representation.words[0];
    row->pending_b = representation.words[1];
}
