/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define ROUTE_TARGET_STATUS  8
#define ROUTE_SLOT_X(i)      EF_U16(0x001a55a2u + 2u * (uint32_t)(i))  /* g_route_slot_x[i] */
#define ROUTE_SLOT_Y(i)      EF_U16(0x001a55d6u + 2u * (uint32_t)(i))  /* g_route_slot_y[i] */

#define EVENT_LIST_A         0x00137960u  /* g_group_list_head */
#define EVENT_LIST_B         0x00137964u
#define EVENT_LIST_C         0x00137988u
#define EVENT_LIST_D         0x00137980u
#define EVENT_ROUTE_SPAWN    0x1b
#define EVENT_ARG_3          3
#define EVENT_ARG_4          4
#define DEFAULT_ROUTE_SIDE   1

#define S16ARG(v)            ((uint32_t)(int32_t)(int16_t)(v))

static uint32_t random_code(void)
{
    return g_route_side == 1 ? random_code_aa6c() : random_step_aaec();
}

/* Spawn one route event at (x, y) and keep its record as the latest. */
static void spawn_route_event(uint32_t code, uint32_t side, int16_t x, int16_t y)
{
    uint32_t delay = random_delay_a9fc();
    int32_t count = pick_count_a9a8();

    g_last_route_event = (uint16_t)spawn_event_record_befb8(
        EVENT_LIST_A, EVENT_LIST_B, EVENT_LIST_C, EVENT_LIST_D,
        EVENT_ROUTE_SPAWN, code & 0xffu, side, EVENT_ARG_3, EVENT_ARG_4,
        S16ARG(count), S16ARG(x), S16ARG(y), S16ARG(delay), S16ARG(g_last_route_event));
}

/* 0xe490, 625 bytes. Find the first route slot with status 8, keep it as
 * the route target and the slot before it as the route origin. Without
 * a selected assignment spawn one route event at the target for side 1.
 * Otherwise spawn two or three events spread around the origin
 * (+-8, +-14) and one or two around the target (+-4), coding each by the
 * route side. */
void spawn_route_events_e490(void)
{
    int16_t slot = 0;
    int16_t count, i;
    int16_t x, y;

    while (g_tactical_status_entries[slot] != ROUTE_TARGET_STATUS)
        slot++;
    g_route_target_x = ROUTE_SLOT_X(slot);
    g_route_target_y = ROUTE_SLOT_Y(slot);
    g_route_origin_x = ROUTE_SLOT_X(slot - 1);
    g_route_origin_y = ROUTE_SLOT_Y(slot - 1);

    if (g_assignment_selected_record == 0) {
        spawn_route_event(random_code_aa6c(), DEFAULT_ROUTE_SIDE, g_route_target_x, g_route_target_y);
        return;
    }

    count = (int16_t)(half_word_9c960() % 2 + 2);
    for (i = 0; i < count; i++) {
        x = (int16_t)random_clamped_ab2c(g_route_origin_x, 8);
        y = (int16_t)random_clamped_ab2c(g_route_origin_y, 14);
        spawn_route_event(random_code(), g_route_side, x, y);
    }

    count = (int16_t)(half_word_9c960() % 2 + 1);
    for (i = 0; i < count; i++) {
        x = (int16_t)random_clamped_ab2c(g_route_target_x, 4);
        y = (int16_t)random_clamped_ab2c(g_route_target_y, 4);
        spawn_route_event(random_code(), g_route_side, x, y);
    }
}
