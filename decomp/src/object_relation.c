/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define GRID_RECORD_SIZE   0xcau
#define GRID_RECORD_KEY    0x15u

/* Object record fields. */
#define OBJECT_FLAGS       0x24u
#define OBJECT_GRID_CELL   0x28u
#define OBJECT_ANCHOR      0x16u
#define OBJECT_SELECTOR    0x146u
#define OBJECT_TARGET      0x158u
#define OBJECT_FLAG_ANCHOR 0x02u
#define OBJECT_FLAG_4      0x04u
#define OBJECT_FLAG_8      0x08u

static uint16_t grid_cell_key(ef_addr32 object)
{
    return EF_U16(g_world_grid_base + GRID_RECORD_SIZE * (uint32_t)(int32_t)EF_S16(object + OBJECT_GRID_CELL)
                  + GRID_RECORD_KEY);
}

/* 0x23800, 160 bytes. True when the current object's target is a live
   object on the selected secondary grid, in a grid cell with a different
   key, without flags 4 or 8 and, when anchored, with its anchor also on
   the secondary grid. */
uint8_t check_current_object_relation(void)
{
    ef_addr32 target = lookup_filtered_indexed_object(EF_S16(g_current_object + OBJECT_TARGET));
    uint8_t flags;

    if (target == 0)
        return 0;
    if ((int32_t)EF_U16(target + OBJECT_SELECTOR) != (int32_t)(int16_t)g_grid_secondary_selector)
        return 0;
    if (grid_cell_key(target) == grid_cell_key(g_current_object))
        return 0;
    flags = EF_U8(target + OBJECT_FLAGS);
    if ((flags & (OBJECT_FLAG_4 | OBJECT_FLAG_8)) != 0)
        return 0;
    if ((flags & OBJECT_FLAG_ANCHOR) != 0 && object_uses_secondary_grid(EF_S16(target + OBJECT_ANCHOR)) == 0)
        return 0;
    return 1;
}
