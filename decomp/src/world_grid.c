/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* The grid pointer is a guest address. For host models the translation is
 * supplied by the test fixture; original guest code uses the raw pointer. */
static ef_world_grid_cell *grid_cell(int16_t index)
{
    uint8_t *base = (uint8_t *)ef_guest_memory_ptr(g_world_grid_base);
    return (ef_world_grid_cell *)(base + (int32_t)index * 0xca);
}

static int16_t cell_object_index(const ef_world_grid_cell *cell)
{
    return (int16_t)(cell->indexed_object_packed >> 16);
}

/* 0x0002b884  size 0x3a  drafted. The signed AX index is compared to an
 * unsigned grid halfword. Negative indices cannot match. Every call to
 * the filtered lookup must remain observable in its diagnostic counter. */
uint8_t grid_slot_matches_object(int16_t index)
{
    ef_addr32 addr = lookup_filtered_indexed_object(index);
    if (addr != 0) {
        const uint8_t *object = (const uint8_t *)ef_guest_memory_ptr(addr);
        int32_t cell_index = (int16_t)((uint16_t)object[0x28] |
                                       ((uint16_t)object[0x29] << 8));
        ef_world_grid_cell *cell = grid_cell((int16_t)cell_index);
        if ((uint32_t)(uint16_t)cell_object_index(cell) ==
            (uint32_t)(int32_t)index) {
            return 1;
        }
    }
    return 0;
}

/* 0x0002b910  size 0x4b  drafted. Dispatch precedes the local byte write;
 * the original does not check the grid row index or the object's validity. */
void set_grid_slot_event(int16_t cell_index, uint8_t value)
{
    ef_world_grid_cell *cell = grid_cell(cell_index);
    int16_t object_index = cell_object_index(cell);
    dispatch_indexed_object_event(object_index, (int16_t)(0x58 + value),
                                  0, 0, 0);
    cell = grid_cell(cell_index); /* original reloads the base after dispatch */
    cell->event_value = value;
}

/* 0x0002b95c  size 0x64  drafted. A disabled grid cell starts an object
 * state transition only if its type's 0xe5-stride flag is nonzero. */
void enable_grid_slot_state(int16_t cell_index)
{
    ef_world_grid_cell *cell = grid_cell(cell_index);
    if (cell->state_enabled == 0 &&
        g_object_type_flags[(uint32_t)cell->object_type * 0xe5] != 0) {
        int16_t object_index = cell_object_index(cell);
        set_indexed_object_state(object_index);
        /* The original rereads the index from the (possibly changed) grid
         * after the state call, before dispatching the event. */
        object_index = cell_object_index(grid_cell(cell_index));
        dispatch_indexed_object_event(object_index, 7, 0, 0, 0);
        grid_cell(cell_index)->state_enabled = 1;
    }
}

/* 0x0002ba14  size 0x56  drafted. A separate requested/active pair
 * controls the activation event (code 8). */
void enable_grid_slot_activation(int16_t cell_index)
{
    ef_world_grid_cell *cell = grid_cell(cell_index);
    if (cell->activation_requested != 0 && cell->activation_active == 0) {
        int16_t object_index = cell_object_index(cell);
        set_indexed_object_activation(object_index);
        object_index = cell_object_index(grid_cell(cell_index));
        dispatch_indexed_object_event(object_index, 8, 0, 0, 0);
        grid_cell(cell_index)->activation_active = 1;
    }
}
