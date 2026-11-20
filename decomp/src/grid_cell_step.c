/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/world_grid.h"

#define GRID_CELL_NONE      0xff
#define CELL_OWNER(cell)    EF_U8(cell)            /* +0x00, owner marker, 0xff = none */
#define CELL_OBJECT_BYTE(c) EF_U8((c) + 0x4fu)     /* byte 1 of the packed object field */
#define CONNECTION_MODE_RECORDING 3u

static ef_addr32 grid_cell(int16_t index)
{
    return g_world_grid_base + (uint32_t)((int32_t)index * (int32_t)sizeof(ef_world_grid_cell));
}

/* 0x2e740, 461 bytes, EAX cell plus a stack double RATE and dword
 * IMMEDIATE, ret 0xc. Step one active grid cell. The rate is stored at
 * 0x1a3140 for the callees. IMMEDIATE (1) resets the cell state and runs
 * 0x2ced4 before the update. In multiplayer a cell owned by another
 * player is only reported (0x546c0) when its object belongs to the local
 * one. Otherwise the selected cell (0x19bb5e) or the reset state +0x20
 * pick one of the advance helpers, then 0x2d488 updates the cell; in
 * multiplayer a non-immediate step of an own or unowned cell is sent
 * through 0x54c10. */
void step_grid_cell_2e740(int32_t cell_index, double rate, uint32_t immediate)
{
    int16_t index = (int16_t)cell_index;
    ef_addr32 cell;
    ef_addr32 obj;

    if (g_frontend_connection_mode == CONNECTION_MODE_RECORDING)
        record_track_sample_2e494();
    if (index == GRID_CELL_NONE)
        return;
    cell = grid_cell(index);
    if (CELL_OBJECT_BYTE(cell) == 0u)
        EF_U8(cell + offsetof(ef_world_grid_cell, current_mode)) = 0u;
    if (EF_U8(cell + offsetof(ef_world_grid_cell, current_mode)) == 0u)
        return;
    g_grid_cell_step_rate = rate;

    if ((uint8_t)immediate == 1u) {
        EF_U8(cell + offsetof(ef_world_grid_cell, reset_state)) = 0u;
        guest_fn_2ced4(index, rate);
        guest_fn_2d488(index, rate);
        return;
    }

    if (g_world_grid_mode != 0u && CELL_OWNER(cell) != g_world_grid_marker) {
        obj = lookup_filtered_indexed_object(
            (int16_t)(EF_S32(cell + offsetof(ef_world_grid_cell, indexed_object_packed)) >> 16));
        if (obj != 0u && EF_U16(obj + 0x18u) == (uint16_t)g_world_grid_marker)
            send_grid_marker_short_packet(index);
        return;
    }

    if (index == g_active_grid_index) {
        if (EF_U8(grid_cell(g_active_grid_index) + offsetof(ef_world_grid_cell, reset_state)) == 0u)
            guest_fn_2e184(index);
        guest_fn_2c778(index);
    } else if (EF_U8(cell + offsetof(ef_world_grid_cell, reset_state)) == 1u) {
        if ((uint8_t)position_clear_2dee4((uint32_t)(int32_t)index) != 0u)
            reset_grid_cell_state(index);
        else
            guest_fn_2c778(index);
    } else if (grid_is_visible_from_reference(index) != 0u) {
        guest_fn_2e184(index);
    } else {
        guest_fn_2ced4(index, rate);
    }
    guest_fn_2d488(index, rate);

    if (g_world_grid_mode == 0u || (uint8_t)immediate != 0u)
        return;
    cell = grid_cell(index);
    if (CELL_OWNER(cell) == g_world_grid_marker || CELL_OWNER(cell) == GRID_CELL_NONE)
        guest_fn_54c10(index);
}
