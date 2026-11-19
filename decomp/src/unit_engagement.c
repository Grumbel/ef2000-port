/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/unit_engagement.h"

/* Carry-chain recipe: recognize signed division's bias before replacing
 * flags. At 0x2a0b1, SAR sign / SHL 2 / SBB / SAR 2 adds three for negative
 * operands, hence signed division by four toward zero. Here the operand is
 * positive: q = 10 / nonzero uint16_t step, so the largest divisor is 1524.
 * AX/DX narrowing and random-state advancement remain explicit below. */

/* 0x29f54, 532 bytes. Rate-limit close-target attack/damage ticks by skill,
 * frame step and whether the unit shares the active grid's locator. */
void update_unit_engagement_29f54(int16_t index)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    ef_engagement_unit *unit, *target;
    int32_t dx, dz, distance, divisor, q;
    uint8_t kind;

    if (address == 0 || g_simulation_tick_step == 0)
        return;
    unit = (ef_engagement_unit *)ef_guest_memory_ptr(address);
    if (unit->target == 0)
        return;
    target = (ef_engagement_unit *)ef_guest_memory_ptr(unit->target);
    dx = (target->x_fixed >> 16) - (unit->x_fixed >> 16);
    dz = (target->z_fixed >> 16) - (unit->z_fixed >> 16);
    distance = (int32_t)abs_cf94a(dx);
    if (distance <= (int32_t)abs_cf94a(dz))
        distance = (int32_t)abs_cf94a(dz);
    if (distance > 160)
        return;

    q = 10 / g_simulation_tick_step;
    divisor = distance > 16 ? 38 * q + 1 : 6 * q + 1;
    if (g_combat_skill_level < 6) {
        int16_t grid = (int16_t)(unit->grid_packed >> 16);
        ef_world_grid_cell *cell = (ef_world_grid_cell *)ef_guest_memory_ptr(
            g_world_grid_base + (uint32_t)(grid * 0xca));
        ef_world_grid_cell *active = (ef_world_grid_cell *)ef_guest_memory_ptr(
            g_world_grid_base + (uint32_t)(g_active_grid_index * 0xca));
        if ((int16_t)(cell->locator_packed >> 16) ==
            (int16_t)(active->locator_packed >> 16))
            divisor = (int16_t)divisor / (g_combat_skill_level < 3 ? 4 : 2);
        else
            divisor *= g_combat_skill_level < 3 ? 4 : 2;
    }
    if ((int16_t)divisor == 0)
        return;
    if ((uint16_t)half_word_9c960() % (int16_t)divisor != 0)
        return;
    kind = g_grid_category_types[target->category].engagement_kind;
    if (kind == 1)
        unit_attack_tick_29b14(index);
    else if (kind == 2)
        unit_damage_tick_29dd0((uint32_t)(int32_t)index);
}
