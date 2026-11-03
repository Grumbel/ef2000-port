/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <math.h>

static ef_world_grid_cell *grid_row(int16_t cell)
{
    return (ef_world_grid_cell *)((uint8_t *)ef_guest_memory_ptr(g_world_grid_base)
                                  + (int32_t)cell * 0xca);
}

static int16_t attached_object(const uint8_t *row, uint16_t entry)
{
    uint16_t pos = (uint16_t)(0x50 + entry * 2);
    return (int16_t)((uint16_t)row[pos] | ((uint16_t)row[pos + 1] << 8));
}

/* 0x0002bbd0  size 0x6c  drafted. Original caller functions 0x2bc3c,
 * 0x2bcb8 and 0x2c6dc. Eight neighbors, including the +0xff sentinel,
 * are examined in order; own-row matches are excluded. */
int16_t find_grid_neighbor_with_detail(int16_t cell)
{
    const ef_world_grid_cell *row = grid_row(cell);
    const ef_grid_adjacency *tables =
        (const ef_grid_adjacency *)ef_guest_memory_ptr(g_grid_adjacency_base);
    int16_t group = row->adjacency_group;
    uint16_t slot;
    for (slot = 0; slot < 8; ++slot) {
        int16_t other = tables[group].neighbors[slot];
        if (other != 0xff && other != cell && grid_row(other)->detail_mode == 1) {
            return other;
        }
    }
    return 0xff;
}

/* 0x0002bb3c  size 0x28  drafted. Six original call sites across 0x71fc,
 * 0x7308, 0x16b0c, 0x2bc3c, 0x2c224 and 0x46454. Original passes the
 * two +0x61/+0x69 double pairs to runtime distance routine 0xb415c;
 * that routine returns sqrt(dx*dx+dy*dy), or zero for nonpositive sums.
 * Host sqrt rounding is not yet validated against the original x87 ST0. */
double grid_cell_distance(int16_t first, int16_t second)
{
    const ef_world_grid_cell *a = grid_row(first);
    const ef_world_grid_cell *b = grid_row(second);
    double dx = (double)((long double)a->position_a - (long double)b->position_a);
    double dy = (double)((long double)a->position_b - (long double)b->position_b);
    long double sum = (long double)dx * dx + (long double)dy * dy;
    /* 0xb415c uses x87 FCOMP/JAE: an unordered (NaN) comparison takes
     * the sqrt path, not the nonpositive-result path. */
    return 0.0L >= sum ? 0.0 : sqrt((double)sum);
}

/* 0x0002bc3c  size 0x7a  drafted. Original callers 0x2ced4 and 0x6e59c.
 * The active row's +0x1c word must match the candidate's before locating
 * a neighbor, and the neighbor must have a nonzero current mode. */
double grid_neighbor_distance(int16_t cell)
{
    if (g_active_grid_index != 0xff) {
        const ef_world_grid_cell *selected = grid_row(g_active_grid_index);
        const ef_world_grid_cell *row = grid_row(cell);
        if (selected->adjacency_group == row->adjacency_group) {
            int16_t neighbor = find_grid_neighbor_with_detail(cell);
            if (neighbor != 0xff && grid_row(neighbor)->current_mode != 0) {
                return grid_cell_distance(neighbor, cell);
            }
        }
    }
    return 0.0;
}

/* 0x0002c0ec  size 0x8f  drafted. Callers 0x2c6dc and 0x77c90 both
 * test AL. The source performs at most fourteen filtered object lookups:
 * for each of seven attached objects, follow its +0x144 reference and
 * compare the two linked grid rows' +0x15 words. */
uint8_t grid_has_cross_object_relation(int16_t cell)
{
    uint16_t slot;
    for (slot = 0; slot < 7; ++slot) {
        const uint8_t *row = (const uint8_t *)grid_row(cell);
        ef_addr32 object = lookup_filtered_indexed_object(attached_object(row, slot));
        if (object != 0) {
            const uint8_t *obj = (const uint8_t *)ef_guest_memory_ptr(object);
            int16_t neighbor_object = (int16_t)((uint16_t)obj[0x146] |
                                                 ((uint16_t)obj[0x147] << 8));
            ef_addr32 neighbor = lookup_filtered_indexed_object(neighbor_object);
            if (neighbor != 0) {
                const uint8_t *other = (const uint8_t *)ef_guest_memory_ptr(neighbor);
                int16_t first_cell = (int16_t)((uint16_t)obj[0x28] |
                                               ((uint16_t)obj[0x29] << 8));
                int16_t second_cell = (int16_t)((uint16_t)other[0x28] |
                                                ((uint16_t)other[0x29] << 8));
                /* +0x15 is the high signed word of locator_packed. */
                if ((uint16_t)((uint32_t)grid_row(first_cell)->locator_packed >> 16) !=
                    (uint16_t)((uint32_t)grid_row(second_cell)->locator_packed >> 16)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}
