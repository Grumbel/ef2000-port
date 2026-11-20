/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define ROUTE_LEGS            24
#define ROUTE_HOME_SLOT       25       /* the route ends back at this point */
#define ROUTE_SLOT_FREE       0x11u    /* g_tactical_status_entries: no waypoint */
#define ROUTE_SLOT_TARGET     8u       /* g_tactical_status_entries: target waypoint */
#define ROUTE_NO_TARGET       100
#define NM_PER_MAP_UNIT       3.0

/* Strings in the data segment. */
#define DIALOG_NAME           0x00184a68u /* "mp_wp_set" */
#define DIST_SUFFIX           0x00184d60u /* " nm" */
#define DIST_WIDGET           0x00184d64u /* "dist_value" */
#define SPEED_SUFFIX          0x00184d70u /* " knts" */
#define SPEED_WIDGET          0x00184d78u /* "speed_value" */
#define TIME_FORMAT           0x00184d84u /* "%.2f hrs" */
#define TIME_WIDGET           0x00184d90u /* "time_value" */

/* Leg length in nautical miles between two route slots. */
static double leg_length(int32_t a, int32_t b)
{
    uint32_t dx = abs_cf94a((int32_t)(uint16_t)g_route_slot_x[a] - (int32_t)(uint16_t)g_route_slot_x[b]);
    uint32_t dy = abs_cf94a((int32_t)(uint16_t)g_route_slot_y[a] - (int32_t)(uint16_t)g_route_slot_y[b]);

    return guest_fn_a4470((double)(dx * dx + dy * dy)) * NM_PER_MAP_UNIT;
}

static uint32_t add_leg(uint32_t total, double leg)
{
    return (uint32_t)(int32_t)truncate_9d72c((double)total + leg);
}

static void show_value(ef_addr32 dialog, ef_addr32 widget, char *text)
{
    show_widget_a7588(find_widget_checked_a78bc(dialog, widget), (ef_addr32)(uintptr_t)text);
}

/* 0x4ec70, 591 bytes. Sum the route legs (whole nautical miles, each
 * running total truncated) from slot 0 until the first free slot or 24
 * legs, then back to the home slot 25. The total up to the target
 * waypoint (status 8) goes to g_route_target_distance, the total before
 * its last leg to g_route_before_target (100 without a target). The
 * flight times at g_route_speed knots are stored as floats, then
 * 0x4f0d4 runs and the mp_wp_set dialog shows the distance, speed and
 * time. EAX is clobbered. */
void update_route_summary_4ec70(void)
{
    uint32_t total = 0;
    uint32_t target = 0;
    char text[20];
    ef_addr32 dialog;
    int16_t i;

    g_route_before_target = ROUTE_NO_TARGET;
    for (i = 0; i < ROUTE_LEGS; ++i) {
        if (g_tactical_status_entries[i + 1] == ROUTE_SLOT_FREE)
            break;
        total = add_leg(total, leg_length(i, i + 1));
        if (g_tactical_status_entries[i + 2] == ROUTE_SLOT_TARGET)
            g_route_before_target = total;
        if (g_tactical_status_entries[i + 1] == ROUTE_SLOT_TARGET)
            target = total;
    }
    total = add_leg(total, leg_length(i, ROUTE_HOME_SLOT));
    g_route_total_hours = (float)((double)total * (1.0 / (double)(int32_t)g_route_speed));
    g_route_target_hours = (float)((double)target * (1.0 / (double)(int32_t)g_route_speed));
    guest_fn_4f0d4();

    dialog = find_named_or_abort_a46cc(DIALOG_NAME);
    signed_to_digits_cbd0f((int32_t)total, (ef_addr32)(uintptr_t)text, 10);
    strcat_9ca57((ef_addr32)(uintptr_t)text, DIST_SUFFIX);
    show_value(dialog, DIST_WIDGET, text);
    signed_to_digits_cbd0f((int32_t)g_route_speed, (ef_addr32)(uintptr_t)text, 10);
    strcat_9ca57((ef_addr32)(uintptr_t)text, SPEED_SUFFIX);
    show_value(dialog, SPEED_WIDGET, text);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, TIME_FORMAT, (double)g_route_total_hours);
    show_value(dialog, TIME_WIDGET, text);
}
