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

/* Route gauge: a float from 0x4f0d4, at most 7000, drawn as a bar of up to
 * 150 columns (one per 47 units). The two 17x6 end icons are copied from a
 * 640-byte wide sheet. */
#define GAUGE_VALUE           0x001a56f8u /* float */
#define GAUGE_SCALE           0x00184da3u /* float 1/47 */
#define GAUGE_COLUMNS         146
#define GAUGE_MAX_LEVEL       150
#define GAUGE_X               12
#define GAUGE_TOP             0x73
#define GAUGE_BOTTOM          0x82
#define ICON_SHEET            0x00126570u /* pointer to the 640-byte wide icon sheet */
#define ICON_A_POS            0x00134727u /* x, y words */
#define ICON_B_POS            0x00134730u

static void draw_gauge_tick(uint32_t colour, uint32_t x)
{
    draw_scaled_rect_c95ac(colour, x, 0x72, x + 6u, GAUGE_BOTTOM);
}

static void draw_gauge_icon(ef_addr32 pos, int32_t a, int32_t b)
{
    int32_t source = (int32_t)EF_U32(ICON_SHEET) + (int16_t)EF_U16(pos + 2u) * 0x280 + (int16_t)EF_U16(pos);

    guest_fn_ca620(source, a, b, 0x11u, 6u, 0x280u);
}

/* 0x4eec0, 532 bytes. Draw the route gauge: a framed bar of 146 one-pixel
 * columns, colour 3 up to the level, switching to 1 at column 110 when the
 * level is beyond it and to 10 past the level; then dark and light tick
 * marks at three positions and the two end icons. */
void mpToggleIcons(void)
{
    int16_t level;
    int16_t colour = 3;
    int16_t i;

    guest_fn_c9a98_x(8, 0x0b, 0x72, 0xa1, 0x84);
    guest_fn_c9a98_x(0xf, 0x0a, 0x71, 0xa0, 0x83);
    level = (int16_t)(int32_t)truncate_9d72c((double)*(float *)ef_guest_memory_ptr(GAUGE_VALUE) *
                                             (double)*(float *)ef_guest_memory_ptr(GAUGE_SCALE));
    if (level > GAUGE_MAX_LEVEL)
        level = GAUGE_MAX_LEVEL;

    for (i = 0; i < GAUGE_COLUMNS; i++) {
        draw_scaled_rect_c95ac((uint8_t)colour, (uint32_t)(GAUGE_X + i), GAUGE_TOP,
                               (uint32_t)(GAUGE_X + i), GAUGE_BOTTOM);
        if (i == 110 && i < level)
            colour = 1;
        if (i > level)
            colour = 10;
    }

    draw_gauge_tick(8, 0x66);
    draw_gauge_tick(8, 0x7b);
    draw_gauge_tick(8, 0x50);
    draw_gauge_tick(0xf, 0x65);
    draw_gauge_tick(0xf, 0x7a);
    draw_gauge_tick(0xf, 0x4f);
    draw_gauge_icon(ICON_A_POS, 0x0d, 0x74);
    draw_gauge_icon(ICON_B_POS, 0x8a, 0x7c);
}

/* Fuel model for 0x4f0d4. The constants live in the data segment. */
#define ROUTE_SLOT_ALT        0x001a560au /* int32_t[26], feet */
#define FEET_TO_METRES        (*(double *)ef_guest_memory_ptr(0x00184da7u)) /* 0.3048 */
#define KNOTS_TO_MPS          (*(double *)ef_guest_memory_ptr(0x00184dafu)) /* 0.515 */
#define DRAG_FACTOR           (*(double *)ef_guest_memory_ptr(0x00184db7u)) /* 30.625 */
#define INDUCED_FACTOR        (*(double *)ef_guest_memory_ptr(0x00184dbfu)) /* 0.133 */
#define DENSITY_SPEED         (*(double *)ef_guest_memory_ptr(0x00184dc7u)) /* 50.0 */
#define RANGE_FACTOR          (*(double *)ef_guest_memory_ptr(0x00184dcfu)) /* 21300.0 */
#define METRES_PER_NM         (*(double *)ef_guest_memory_ptr(0x00184ddfu)) /* 1850.0 */
#define FUEL_TO_GAUGE         (*(double *)ef_guest_memory_ptr(0x00184de7u)) /* 1/9.81 */
#define DENSITY_SCALE         (*(double *)ef_guest_memory_ptr(0x00184defu)) /* 20000.0 */
#define MIN_DENSITY           (*(double *)ef_guest_memory_ptr(0x00184df7u)) /* 0.075 */
#define FULL_FUEL             206010
#define SLOT_REFUEL           5u       /* g_tactical_status_entries: tanker */
#define EMPTY_GAUGE_VALUE     7000.0f

/* Same as leg_length, but the squared length is converted as a signed
 * integer, as the original's fild does. */
static double leg_metres(int32_t a, int32_t b)
{
    uint32_t dx = abs_cf94a((int32_t)(uint16_t)g_route_slot_x[a] - (int32_t)(uint16_t)g_route_slot_x[b]);
    uint32_t dy = abs_cf94a((int32_t)(uint16_t)g_route_slot_y[a] - (int32_t)(uint16_t)g_route_slot_y[b]);

    return guest_fn_a4470((double)(int32_t)(dx * dx + dy * dy)) * NM_PER_MAP_UNIT * METRES_PER_NM;
}

/* 0x4f0d4, 547 bytes. Fly the route from slot 0 with full fuel (206010)
 * at g_route_speed: per leg the air density ratio at the slot's altitude,
 * (20000 - h) / (20000 + h) with h in metres and at least 0.075, gives
 * the lift coefficient and so the drag and the fuel-limited range; the
 * leg's length is taken off the square root of the remaining fuel. A
 * tanker slot (status 5) refills and the target (8) lowers the drag
 * coefficient. Running dry sets the gauge at 0x1a56f8 to 7000, otherwise
 * it gets the fuel used over g. Registers are preserved. */
void route_fuel_4f0d4(void)
{
    double fuel = (double)FULL_FUEL;
    float base_drag = 0.06f;
    uint8_t dry = 0;
    int16_t i;

    for (i = 0; i < ROUTE_HOME_SLOT; ++i) {
        int16_t next = i + 1;
        double density, speed, lift, drag, range, left;

        if (g_tactical_status_entries[i] == ROUTE_SLOT_FREE)
            break;
        if (g_tactical_status_entries[i + 1] == ROUTE_SLOT_FREE)
            next = ROUTE_HOME_SLOT;
        if (g_tactical_status_entries[i] == SLOT_REFUEL)
            fuel = (double)FULL_FUEL;
        if (g_tactical_status_entries[i] == ROUTE_SLOT_TARGET)
            base_drag = 0.035f;

        density = (double)(int32_t)EF_U32(ROUTE_SLOT_ALT + (uint32_t)i * 4u) * FEET_TO_METRES;
        density = (DENSITY_SCALE - density) / (DENSITY_SCALE + density);
        if (density < MIN_DENSITY)
            density = 0.075;

        speed = (double)(int32_t)g_route_speed * KNOTS_TO_MPS;
        lift = fuel / (density * DRAG_FACTOR * speed * speed);
        drag = lift * INDUCED_FACTOR * lift + (double)base_drag;
        range = 1.0 / guest_fn_a4470(density * DENSITY_SPEED) * RANGE_FACTOR;
        range = guest_fn_a4470(lift) / drag * range;

        left = guest_fn_a4470(fuel) - leg_metres(i, next) / range;
        if (0.0 > left)
            dry = 1;
        fuel = left * left;
        if (dry) {
            *(float *)ef_guest_memory_ptr(GAUGE_VALUE) = EMPTY_GAUGE_VALUE;
            return;
        }
    }
    *(float *)ef_guest_memory_ptr(GAUGE_VALUE) = (float)(((double)FULL_FUEL - fuel) * FUEL_TO_GAUGE);
}
