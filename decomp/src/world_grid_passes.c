/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/world_grid.h"

#define GRID_UNIT_SLOTS 6

enum grid_unit_pass { FIRST_UNIT_PASS, SECOND_UNIT_PASS, RETARGET_UNIT_PASS };

static void run_unit_pass(enum grid_unit_pass pass, int16_t unit)
{
    switch (pass) {
    case FIRST_UNIT_PASS:
        guest_fn_8d59c(unit);
        break;
    case SECOND_UNIT_PASS:
        guest_fn_8d3d4(unit);
        break;
    default:
        maybe_retarget_8d554(unit);
        break;
    }
}

/* Run PASS for the six unit words at +0x50 of every armed cell (reset
 * state 1), skipping the sentinel row 255. The count is reread after
 * every cell. */
static void for_each_armed_unit(enum grid_unit_pass pass)
{
    int16_t row, slot;

    for (row = 0; row < g_unit_count; ++row) {
        ef_addr32 cell;

        if (row == 0xff)
            continue;
        cell = g_world_grid_base + (uint32_t)((int32_t)row * (int32_t)sizeof(ef_world_grid_cell));
        if (((const ef_world_grid_cell *)(uintptr_t)cell)->reset_state != 1u)
            continue;
        for (slot = 0; slot < GRID_UNIT_SLOTS; ++slot) {
            cell = g_world_grid_base + (uint32_t)((int32_t)row * (int32_t)sizeof(ef_world_grid_cell));
            run_unit_pass(pass, EF_S16(cell + 0x50u + (uint32_t)slot * 2u));
        }
    }
}

/* 0x8dd48, 339 bytes. Per-frame unit passes over the armed grid cells:
 * clear g_grid_event_pending, run 0x8d59c, then 0x8d3d4, then
 * maybe_retarget_8d554 over every armed unit, and finally check the cell
 * triggers. */
void run_grid_unit_passes_8dd48(void)
{
    g_grid_event_pending = 0;
    for_each_armed_unit(FIRST_UNIT_PASS);
    for_each_armed_unit(SECOND_UNIT_PASS);
    for_each_armed_unit(RETARGET_UNIT_PASS);
    check_cell_trigger_8d030();
}
