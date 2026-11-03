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

static ef_external_grid_node *external_node(int16_t index)
{
    return (ef_external_grid_node *)((uint8_t *)ef_guest_memory_ptr(g_external_grid_base)
                                     + (int32_t)index * 0x45);
}

/* 0x0002afcc  size 0xab  drafted. Called from 0x2b0b4 and 0x2b1f4.
 * The original uses literal 0xff as the absent-link sentinel, not -1. */
void detach_grid_link_on_mode_change(int16_t cell, uint8_t next_mode)
{
    ef_world_grid_cell *row = grid_row(cell);
    if (row->current_mode == 3) {
        if (row->link_state == 0) {
            int16_t next_cell = row->forward_link;
            if (next_cell != 0xff) {
                ef_world_grid_cell *other = grid_row(next_cell);
                if (other->reverse_link == cell && next_mode != 3) {
                    other->reverse_link = 0xff;
                    row->forward_link = 0xff;
                    row->link_state = 0xff;
                }
            }
        }
    } else if (row->current_mode == 6) {
        if ((row->detail_mode == 6 || row->detail_mode == 7) &&
            cell != g_active_grid_index) {
            grid_row(cell)->link_pending_marker = 0xff;
        }
    }
}

/* 0x0002b5c0  size 0x74  drafted. Original callers 0x46454 and 0x47764.
 * The external record stores only the low byte of the source cell index. */
void link_external_grid_cell(int16_t cell, int16_t node)
{
    ef_world_grid_cell *row = grid_row(cell);
    row->link_state = 1;
    row->forward_link = node;
    if (g_world_grid_mode == 0 ||
        external_node(node)->marker == g_world_grid_marker) {
        external_node(node)->back_reference = (uint8_t)cell;
    }
    set_grid_link_state(cell, 3);
    refresh_external_grid_position(cell);
}

/* 0x0002b634  size 0x5b  drafted. Copies the linked row's position
 * into this row's inherited position. Caller checks: 0x72fd/0x2b5b5. */
void copy_grid_link_position(int16_t cell)
{
    ef_world_grid_cell *row = grid_row(cell);
    ef_world_grid_cell *source = grid_row(row->forward_link);
    row->inherited_position_a = source->position_a;
    row->inherited_position_b = source->position_b;
    row->inherited_position_c = source->position_c;
}

/* 0x0002b690  size 0x76  drafted. 0x2b5c0 and 0x46454 call it.
 * Three signed 32-bit external coordinates are stored at +0x0f/+0x07/+0x0b
 * respectively as scaled 64-bit doubles in the world-grid row. */
void refresh_external_grid_position(int16_t cell)
{
    ef_world_grid_cell *row = grid_row(cell);
    ef_external_grid_node *source = external_node(row->forward_link);
    row->inherited_position_a = (double)((long double)source->component_c *
                                          (long double)g_external_grid_scale);
    row->inherited_position_b = (double)((long double)source->component_a *
                                          (long double)g_external_grid_scale);
    row->inherited_position_c = (double)((long double)source->component_b *
                                          (long double)g_external_grid_scale);
}
