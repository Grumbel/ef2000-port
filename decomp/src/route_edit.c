/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mouse state word +0xa7 of the map widget. */
#define WIDGET_MOUSE          0xa7u
#define MOUSE_LEFT_PRESS      0x0010u
#define MOUSE_LEFT_DRAG       0x0008u
#define MOUSE_RIGHT_PRESS     0x1000u

#define MAP_COORD_MAX         0x18f
#define ROUTE_SLOTS           25
#define ROUTE_SLOT_FREE       0x11u    /* g_tactical_status_entries value */
#define ROUTE_NO_SLOT         0x32u    /* g_route_selected_slot: nothing picked */

static int16_t clamp_map_coord(int16_t v)
{
    if (v < 0)
        v = 0;
    if (v > MAP_COORD_MAX)
        v = MAP_COORD_MAX;
    return v;
}

/* Move the selected waypoint to (X, Y) and refresh the route. */
static void place_selected_waypoint(int16_t x, int16_t y)
{
    g_target_list_changed = 1;
    g_route_slot_x[g_route_selected_slot] = x;
    g_route_slot_y[g_route_selected_slot] = y;
    click_panel_slot_4e0d0((uint32_t)(int32_t)x, (uint32_t)(int32_t)y);
    pick_target_at_4e204(x, y);
    update_counter_widget_4e7b8(1);
    rebuild_waypoint_speed_4ec70();
}

/* 0x4dcd8, 520 bytes. Route editing on the mission map (map mode 4) at
 * the clamped GUI cursor. A left press selects the waypoint under the
 * cursor; on a route segment instead it inserts a midpoint after the
 * segment's first waypoint and returns, and on empty map it selects
 * nothing (0x32). Dragging moves the selected waypoint. A right press on
 * a waypoint deletes it (0x4e6bc); on empty map it places the first free
 * slot (state 0x11) there. Every change sets g_target_list_changed and
 * rebuilds the route speeds. */
void adjust_route_view_4dcd8(ef_addr32 widget)
{
    int16_t x = clamp_map_coord(g_gui_cursor_x);
    int16_t y = clamp_map_coord(g_gui_cursor_y);
    int16_t slot;

    if (EF_U16(widget + WIDGET_MOUSE) & MOUSE_LEFT_PRESS) {
        slot = (int16_t)find_icon_at_4dee0((uint32_t)(int32_t)x, (uint32_t)(int32_t)y);
        if (slot == -1) {
            slot = (int16_t)find_slot_at_point_4dfb4((uint32_t)(int32_t)x, (uint32_t)(int32_t)y);
            if (slot != -1) {
                g_target_list_changed = 1;
                g_route_selected_slot = (uint8_t)(slot - 1);
                insert_midpoint_4e5bc();
                update_counter_widget_4e7b8(1);
                rebuild_waypoint_speed_4ec70();
                return;
            }
            g_route_selected_slot = ROUTE_NO_SLOT;
        } else {
            g_route_selected_slot = (uint8_t)slot;
        }
    }
    if ((EF_U16(widget + WIDGET_MOUSE) & MOUSE_LEFT_DRAG) && g_route_selected_slot != ROUTE_NO_SLOT)
        place_selected_waypoint(x, y);
    if ((EF_U16(widget + WIDGET_MOUSE) & MOUSE_RIGHT_PRESS) == 0)
        return;

    slot = (int16_t)find_icon_at_4dee0((uint32_t)(int32_t)x, (uint32_t)(int32_t)y);
    if (slot != -1) {
        g_target_list_changed = 1;
        g_route_selected_slot = (uint8_t)slot;
        shift_queue_4e6bc();
        rebuild_waypoint_speed_4ec70();
        return;
    }
    g_target_list_changed = 1;
    {
        int16_t k;

        for (k = 1; k < ROUTE_SLOTS; ++k) {
            if (g_tactical_status_entries[k] == ROUTE_SLOT_FREE) {
                slot = k;
                break;
            }
        }
    }
    if (slot == -1)
        return;
    g_route_selected_slot = (uint8_t)slot;
    place_selected_waypoint(x, y);
}
