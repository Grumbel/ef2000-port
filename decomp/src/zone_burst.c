/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define BURST_ZONE_SIZE     16u      /* low x, low y, high x, high y (int32) */
#define BURST_HEIGHT_LOW    (-2500)  /* g_player_height window, negative is up */
#define BURST_HEIGHT_HIGH   (-1150)
#define BURST_ANGLE_STEP    50
#define BURST_RANGE_MASK    0x7f
#define BURST_NEAR          10       /* closer bursts push the player's unit */
#define BURST_DAMAGE        4        /* closer still damage it */
#define BURST_LIFT_MASK     0x3f

static int32_t spread(int32_t limit, int32_t offset)
{
    return (limit - (int32_t)abs_a28f2(offset)) * 10;
}

/* One burst at a pseudo-random point around the player. */
static void burst_near_player(void)
{
    uint16_t seed = g_burst_seed;
    int16_t s = (int16_t)seed;
    int16_t offset[2];
    int32_t pos[3];
    int32_t lift;
    int16_t range;

    g_burst_origin_x = g_player_x;
    g_burst_origin_y = g_player_y;
    g_burst_cooldown = (uint16_t)(seed & 1u);
    g_burst_seed = (uint16_t)(seed + (uint16_t)g_burst_angle);
    g_burst_angle = (int16_t)((uint16_t)g_burst_angle + (uint16_t)(s >> 1) + (uint16_t)((seed & 1u) << 15));
    load_sincos_dc7bc(seed);
    g_burst_angle = (int16_t)(g_burst_angle + BURST_ANGLE_STEP);
    rotate_point_dc7e4((ef_addr32)(uintptr_t)offset, 0, (int16_t)(g_burst_angle & BURST_RANGE_MASK));
    lift = (int32_t)(seed & BURST_LIFT_MASK) - 32;
    pos[0] = g_player_x + offset[0];
    pos[1] = g_player_height + lift;
    pos[2] = g_player_y + offset[1];

    range = (int16_t)(g_burst_angle & BURST_RANGE_MASK);
    if (range < BURST_NEAR) {
        uint32_t unit = (uint32_t)((int32_t)g_grid_secondary_selector >> 16);
        double push[3];
        double size[3];

        if (range < BURST_DAMAGE)
            raise_random_counters_74ed8(unit, (uint32_t)(int32_t)range);
        size[0] = (double)spread(BURST_RANGE_MASK, offset[0]);
        size[1] = (double)spread(BURST_RANGE_MASK, offset[1]);
        size[2] = (double)spread(32, lift);
        push[0] = (double)offset[0];
        push[1] = (double)offset[1];
        push[2] = (double)lift;
        apply_impulse_74d98(unit, (ef_addr32)(uintptr_t)push, (ef_addr32)(uintptr_t)size);
    }
    queue_grid_node_2a8ac(0, 0, 0, 0, 0, (ef_addr32)(uintptr_t)pos);
}

/* 0x2a4c8, 600 bytes. While the player flies between heights 1150 and
 * 2500 (g_player_height -1150..-2500) and above the scene gate height,
 * count the burst cooldown down; once it is zero, the first burst zone
 * (rectangles at g_secondary_parser_text, g_burst_zone_count of them)
 * that contains the player gets a burst: a grid node queued at a point
 * offset from the player by the angle sequence in g_burst_seed and
 * g_burst_angle. A burst within range 10 also pushes the player's unit
 * (0x74d98), within 4 it raises its damage counters (0x74ed8). */
void spawn_zone_burst_2a4c8(void)
{
    int16_t i;

    if (g_burst_zone_count == 0)
        return;
    if (g_player_height > BURST_HEIGHT_HIGH || g_player_height < BURST_HEIGHT_LOW)
        return;
    if (g_scene_gate_enabled != 0 && (g_scene_gate_target_packed >> 16) > g_player_height)
        return;
    if (g_burst_cooldown != 0)
        --g_burst_cooldown;
    for (i = 0; g_burst_cooldown == 0 && i < g_burst_zone_count; ++i) {
        ef_addr32 zone = g_secondary_parser_text + (uint32_t)((int32_t)i * (int32_t)BURST_ZONE_SIZE);

        if (point_range_contains_limits(EF_S32(zone), EF_S32(zone + 4u), EF_S32(zone + 8u), EF_S32(zone + 12u))) {
            burst_near_player();
            break;
        }
    }
}
