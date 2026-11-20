/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define GRID_RECORD_SIZE      0xca
#define WAYPOINT_RECORD_SIZE  25
#define WAYPOINT_INVALID      0xff
#define RADAR_ALTITUDE_LIMIT  5000  /* above this the barometric altitude is shown */
#define FINE_ALTITUDE_LIMIT   1000  /* barometric altitude above this rounds to 100 */
#define PERCENT_MAX           100
#define COCKPIT_MODE_FLIGHT   3

#define g_waypoint_kind_values ((ef_addr32)0x0012bc3cu) /* uint32_t[], by waypoint kind */
#define g_avionics_error_hook  EF_U32(0x00132a74u)      /* cdecl message sink */
#define MSG_INVALID_WP_NUM     0x00180310u /* "avionics.c : GetStandardInfo : display_wp_num INVALID" */
#define PERCENT_SCALE          EF_F64(0x00180346u)  /* 100.0 */

static ef_addr32 grid_record(int16_t index)
{
    return g_world_grid_base + (uint32_t)((int32_t)index * GRID_RECORD_SIZE);
}

/* Entering the flight cockpit mode clears runtime slot 7/0 and sets slots
 * 12/1 and 1/2 (0x12d90 toggles a slot). */
static void enter_cockpit_mode(void)
{
    if (read_runtime_slot_flag(7, 0))
        guest_fn_12d90(7, 0);
    if (!read_runtime_slot_flag(12, 1))
        guest_fn_12d90(12, 1);
    if (!read_runtime_slot_flag(1, 2))
        guest_fn_12d90(1, 2);
}

/* Radar altitude while it is at most 5000, otherwise the barometric
 * altitude (above 1000 rounded down to 100); always rounded down to 10
 * and at least 0. Sets g_info_radar_altitude accordingly. */
static int32_t displayed_altitude(void)
{
    int32_t height = -g_player_height;
    int32_t ground = (int32_t)cell_distance_at_7b16c((uint32_t)(int32_t)(int16_t)(g_scene_metric_ref_packed >> 16),
                                                     g_player_x, g_player_y);
    int32_t altitude = (height - ground) * 8;

    g_info_radar_altitude = 1;
    if (altitude > RADAR_ALTITUDE_LIMIT) {
        g_info_radar_altitude = 0;
        altitude = height * 8;
    }
    if (altitude > FINE_ALTITUDE_LIMIT && g_info_radar_altitude == 0)
        altitude = altitude / 100 * 100;
    altitude = altitude / 10 * 10;
    if (altitude < 0)
        altitude = 0;
    return altitude;
}

/* 0x6460, 610 bytes. avionics.c GetStandardInfo: refresh the standard
 * avionics readouts. With a watched unit, take its target id (-1 in a linked
 * solo session), the active grid record's waypoint number and the
 * value for that waypoint's kind (reporting an invalid previous number).
 * Track cockpit-mode changes, then set the displayed altitude, the
 * percentage readout (0..100, nonzero values rounded up), the heading,
 * the range (0x67d4) and the remaining readouts (0x6924). */
void get_standard_info_6460(void)
{
    ef_addr32 unit = lookup_filtered_indexed_object(g_watched_unit);
    int16_t percent;

    if (unit != 0) {
        ef_addr32 rec;
        uint8_t wp;

        if (g_linked_session != 0 && g_kots_lock == 1)
            g_info_target_id = 0xffff;
        else
            g_info_target_id = EF_U16(grid_record((int16_t)(EF_S32(unit + 0x26) >> 16)) + 0x15);
        if (g_info_waypoint_num == WAYPOINT_INVALID)
            guest_callptr1_x(g_avionics_error_hook, MSG_INVALID_WP_NUM);
        rec = grid_record(g_active_grid_index);
        wp = EF_U8(rec + 0x8e);
        g_info_waypoint_num = wp;
        g_info_waypoint_value = EF_U32(g_waypoint_kind_values +
            4u * EF_U8(EF_U32(rec + 0x8a) + (uint32_t)wp * WAYPOINT_RECORD_SIZE + 0x18));
    }
    if (g_info_cockpit_mode != g_cockpit_mode) {
        g_info_cockpit_mode = g_cockpit_mode;
        if (g_cockpit_mode == COCKPIT_MODE_FLIGHT)
            enter_cockpit_mode();
    }
    g_info_altitude = displayed_altitude();

    percent = (int16_t)(int32_t)truncate_9d72c(g_gauge_fraction * PERCENT_SCALE);
    if (percent != 0)
        percent++;
    g_info_percent = percent;
    if (g_info_percent > PERCENT_MAX)
        g_info_percent = PERCENT_MAX;

    g_info_heading = (int16_t)heading_to_current_object_66fc();
    measure_view_range_67d4();
    guest_fn_6924();
}
