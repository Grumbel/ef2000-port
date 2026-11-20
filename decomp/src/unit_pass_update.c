/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/world_grid.h"

#define OBJECT_FLAGS        0x24u  /* bit 3: inactive */
#define OBJECT_STATE_BITS   0x26u
#define OBJECT_GRID_ROW     0x28u
#define OBJECT_OWNER        0x18u
#define OBJECT_MOVE_MODE    0x16cu
#define OBJECT_PANEL        0x20du /* strip word, then a copy of the position and angles */
#define OBJECT_PANEL_SHOWN  0x22fu
#define OBJECT_PANEL_SOURCE 0x230u
#define OWNER_NONE          0xff

static int16_t player_unit(void)
{
    return (int16_t)(g_grid_secondary_selector >> 16);
}

/* The object is the local player's or unowned. */
static int locally_owned(ef_addr32 object)
{
    int16_t owner = EF_S16(object + OBJECT_OWNER);

    return (uint16_t)owner == (uint16_t)g_world_grid_marker || owner == OWNER_NONE;
}

/* 0x8d3d4, 384 bytes. Second per-frame pass for UNIT (from
 * run_grid_unit_passes_8dd48). Units without an active object in an
 * active grid cell are skipped. The player's unit takes a state
 * snapshot; other units with a movement mode other than 0 and 12 move
 * (locally owned) or advance their position. A locally owned, active
 * unit in grid mode 1 is reported with 0x548e0 and loses state bit 5.
 * Finally the panel source word decides the side panel: 1 shows it
 * (allocating a strip once) with the object's position and angles,
 * otherwise a shown panel is cleared. */
void update_unit_pass_8d3d4(int16_t unit)
{
    ef_addr32 object = lookup_filtered_indexed_object(unit);
    ef_addr32 source;
    int16_t row;
    uint8_t mode;

    if (object == 0u || (EF_U8(object + OBJECT_FLAGS) & 8u) != 0u)
        return;
    row = EF_S16(object + OBJECT_GRID_ROW);
    if (((const ef_world_grid_cell *)(uintptr_t)(g_world_grid_base + (uint32_t)((int32_t)row * (int32_t)sizeof(ef_world_grid_cell))))->reset_state == 0u)
        return;
    if (unit == player_unit())
        snapshot_unit_state_7222c(unit);
    mode = EF_U8(object + OBJECT_MOVE_MODE);
    if (mode != 0u && mode != 0xcu && unit != player_unit()) {
        if (locally_owned(object))
            move_unit_72390((uint32_t)(int32_t)unit);
        else
            advance_object_position_724a8((uint32_t)(int32_t)unit, object);
    }
    if (locally_owned(object) && (EF_U8(object + OBJECT_FLAGS) & 8u) == 0u && g_world_grid_mode == 1u) {
        guest_fn_548e0((int32_t)unit);
        EF_U8(object + OBJECT_STATE_BITS) &= 0xdfu;
    }
    source = EF_U32(object + OBJECT_PANEL_SOURCE);
    if (source == 0u)
        return;
    if (EF_S16(source) == 1) {
        int i;

        if (EF_U8(object + OBJECT_PANEL_SHOWN) == 0u) {
            EF_U8(object + OBJECT_PANEL_SHOWN) = 1u;
            EF_U16(object + OBJECT_PANEL) = advance_panel_strip();
        }
        for (i = 0; i < 3; ++i)
            EF_U16(object + OBJECT_PANEL + 0x10u + (uint32_t)i * 2u) = EF_U16(object + 0xcu + (uint32_t)i * 2u);
        for (i = 0; i < 3; ++i)
            EF_U32(object + OBJECT_PANEL + 2u + (uint32_t)i * 4u) = EF_U32(object + (uint32_t)i * 4u);
        register_update_message_80b14(object + OBJECT_PANEL);
    } else if (EF_U8(object + OBJECT_PANEL_SHOWN) == 1u) {
        clear_panel_strip(EF_S16(object + OBJECT_PANEL));
        EF_U8(object + OBJECT_PANEL_SHOWN) = 0u;
    }
}
