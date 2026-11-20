/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/follower.h"

static ef_follower_record *follower(int16_t index)
{
    return (ef_follower_record *)(uintptr_t)(g_external_grid_base + (uint32_t)((int32_t)index * 0x45));
}

/* 0x37eac, 309 bytes. Advance follower INDEX by VALUE. The clock in
 * g_follower_clock is first set to the reference time in seconds. Rows of
 * another grid are left alone. An active row is advanced directly
 * (0x37fe4) when IMMEDIATE is set. Otherwise a following row (state 1)
 * is updated (0x381a0) and stops following once every subitem accepts
 * it, and any other row is advanced and starts following (0x37934) once
 * any subitem accepts it. Unless IMMEDIATE is set, the alternate grid
 * mode then runs the high-level hook 0x54b9c. */
void update_follower_37eac(int16_t index, double value, uint32_t immediate)
{
    const int16_t *clock = (const int16_t *)g_reference_clock;
    ef_follower_record *record;

    g_follower_clock = (int32_t)clock[0] * 3600 + (int32_t)clock[1] * 60 + clock[2];
    record = follower(index);
    if (record->marker != g_world_grid_marker)
        return;
    if (record->active != 0u) {
        if ((uint8_t)immediate != 0u) {
            guest_fn_37fe4_x(index, value);
            return;
        }
        if (record->state == 1u) {
            update_follower_381a0((uint32_t)(int32_t)index);
            if (all_accept_37e60(index))
                follower(index)->state = 0u;
        } else {
            guest_fn_37fe4_x(index, value);
            if (any_accepts_37d3c(index)) {
                record = follower(index);
                record->state = 1u;
                guest_fn_37934((ef_addr32)(uintptr_t)record);
            }
        }
    }
    if (g_world_grid_mode != 0u && (uint8_t)immediate == 0u && follower(index)->level >= 0x1bu)
        nop_54b9c();
}
