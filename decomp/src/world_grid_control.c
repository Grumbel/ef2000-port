/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static ef_world_grid_cell *grid_cell(int16_t cell)
{
    return (ef_world_grid_cell *)((uint8_t *)ef_guest_memory_ptr(g_world_grid_base)
                                  + (int32_t)cell * 0xca);
}

/* 0x0002b558  size 0x65  drafted. The first row's link_state/forward_link
 * fields are updated before a possible back-link. The original updates
 * the back-link when mode is zero or the neighbor's tag matches a global. */
void link_grid_cell(int16_t cell, int16_t neighbor)
{
    ef_world_grid_cell *row = grid_cell(cell);
    row->link_state = 0;
    row->forward_link = neighbor;

    if (g_world_grid_mode == 0 ||
        ((const uint8_t *)grid_cell(neighbor))[0] == g_world_grid_marker) {
        grid_cell(neighbor)->reverse_link = cell;
    }

    set_grid_link_state(cell, 3);
    copy_grid_link_position(cell);
}

/* 0x0002e030  size 0x5a  drafted. Row 0xff is the only rejected sentinel.
 * Reloading the grid pointer after the derived reset is observable if that
 * subroutine changes global state. Original stores are two 32-bit words. */
void reset_grid_cell_state(int16_t cell)
{
    if (cell != 0xff) {
        ef_world_grid_cell *row = grid_cell(cell);
        row->reset_state = 0;
        reset_grid_cell_derived(cell);
        row = grid_cell(cell);
        row->retained_a = row->pending_a;
        row->retained_b = row->pending_b;
        if (g_world_grid_mode != 0) {
            update_grid_visual_state(cell);
        }
    }
}
