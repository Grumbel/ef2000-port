/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Mission-planner GUI callbacks recovered for the hybrid build. They are
 * kept apart from mission_menu_callbacks.c, which the host unit test
 * links without the guest globals and callees these use. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Shared body of the mission-role buttons 0x4f3e0/0x4f494/0x4f548/0x4f608.
 * The widget name (bytes +0x42/+0x43) is the button number "1".."16";
 * a second digit after "1" selects 10..19. A number below the roster
 * count toggles ROLE on that entry: an entry already holding ROLE is
 * cleared, otherwise it takes ROLE only while fewer than four entries
 * hold it. The button row, the button labels and the active dialog
 * slots are then refreshed. The index stays a signed 16-bit value as in
 * the original, so a name below '0' reaches entries before the table.
 * EAX, as in the original, is the roster count on the early
 * exits, the holder count when the cap refuses, else the slot flush
 * result. */
static uint32_t toggle_mission_role(ef_addr32 widget, uint8_t role, int check_target)
{
    int16_t index = (int16_t)(EF_U8(widget + 0x42u) - 0x30u);
    int16_t holders;
    int16_t i;

    if (index == 1) {
        uint8_t digit = EF_U8(widget + 0x43u);
        if (digit >= '0' && digit <= '9')
            index = (int16_t)(digit - 0x26u);
    }
    if (index >= (int16_t)g_assignment_entry_count)
        return g_assignment_entry_count;
    if (check_target && g_assignment_target_record != 0u)
        return g_assignment_entry_count;
    if (g_tactical_lookup_codes[index] == role) {
        g_tactical_lookup_codes[index] = 0u;
    } else {
        holders = 0;
        for (i = 0; i < (int16_t)g_assignment_entry_count; ++i)
            if (g_tactical_lookup_codes[i] == role)
                ++holders;
        if (holders >= 4)
            return (uint32_t)(int32_t)holders;
        g_tactical_lookup_codes[index] = role;
    }
    select_button_state_4f718((uint8_t)index, g_tactical_lookup_codes[index]);
    fill_button_widgets_4f7d0();
    return flush_active_dialog_slots();
}

/* 0x4f3e0, 180 bytes, callback slot 0x12ba78: role 3. */
uint32_t mpMission2Click(ef_addr32 widget)
{
    return toggle_mission_role(widget, 3u, 0);
}

/* 0x4f494, 180 bytes, callback slot 0x12ba80: role 4. */
uint32_t mpMission3Click(ef_addr32 widget)
{
    return toggle_mission_role(widget, 4u, 0);
}

/* 0x4f548, 192 bytes, callback slot 0x12ba88: role 1, refused while a
 * target record (dword 0x1a54fa) is selected. */
uint32_t mpMission4Click(ef_addr32 widget)
{
    return toggle_mission_role(widget, 1u, 1);
}

/* 0x4f608, 180 bytes, callback slot 0x12ba90: role 2. */
uint32_t mpClickATC(ef_addr32 widget)
{
    return toggle_mission_role(widget, 2u, 0);
}

/* 0x4d920, 169 bytes, SAM-coverage button WIDGET (decimal number at
 * +0x3f). Pick the number-th entry of the selected record's 0x15-byte
 * table that is not a 0xff/0xfe placeholder (the 16-bit counter is not
 * range checked, as in the original), remember its index and the
 * widget, and roll the coverage: width 0x2000 when the full-coverage
 * flag is set, else 0x800 + rand % 0x3000; range 10000 + rand % 30000. */
void mpSamCoverage(ef_addr32 widget)
{
    uint16_t remaining = (uint16_t)guest_convert_parser_integer(widget + 0x3fu);
    uint32_t index = 0;
    uint8_t kind;

    for (;; ++index) {
        kind = EF_U8(EF_U32(g_assignment_selected_record + 0x2cu) +
                     (uint32_t)((int32_t)(int16_t)index * 0x15));
        if (kind != 0xffu && kind != 0xfeu && --remaining == 0u)
            break;
    }
    if (g_sam_full_coverage == 0u)
        g_sam_coverage_width = (int16_t)(0x800 + (int32_t)(uint16_t)half_word_9c960() % 0x3000);
    else
        g_sam_coverage_width = 0x2000;
    g_assignment_current_byte = (uint8_t)index;
    g_assignment_current_value = widget;
    g_sam_coverage_range = 10000 + (int32_t)(uint16_t)half_word_9c960() % 30000;
}

/* 0x4f994, 206 bytes, flight button WIDGET of the "mp_atc_list" dialog
 * (digit at +0x40). Clear the list's widgets; a slot without a flight
 * (-1) reports the mission-time warning, otherwise enable its "t<n>",
 * "s<n>", "l<n>" and "w<n>" widgets and select the slot. */
void mpFlyMission(ef_addr32 widget)
{
    char name[16];
    ef_addr32 list = find_named_or_abort_a46cc(0x00184b58u);   /* "mp_atc_list" */
    int16_t slot;

    clear_children_dirty(list);
    slot = (int16_t)(EF_U8(widget + 0x40u) - 0x30u);
    if (g_atc_slot_flights[slot] == -1) {
        update_dialog_bindings(0x00132410u, 0x0013243au); /* mission-time warning texts */
        return;
    }
    guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x00184e78u, (int32_t)slot); /* "t%d" */
    show_named_widget_a7318(list, (ef_addr32)(uintptr_t)name);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x00184e7cu, (int32_t)slot); /* "s%d" */
    show_named_widget_a7318(list, (ef_addr32)(uintptr_t)name);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x00184e80u, (int32_t)slot); /* "l%d" */
    show_named_widget_a7318(list, (ef_addr32)(uintptr_t)name);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x00184e84u, (int32_t)slot); /* "w%d" */
    show_named_widget_a7318(list, (ef_addr32)(uintptr_t)name);
    g_atc_selected_slot = (uint8_t)slot;
}

/* 0x4595c, 245 bytes, map click callback of WIDGET (mode flags in the
 * low byte of word +0xa7). Ignored while input byte 0x121e8c is set.
 * The cursor is clamped to 12..387 on both axes. Flag 0x10 selects the
 * marker under the cursor; flag 0x08 moves the selected marker (25-byte
 * entries at +0x20 of the current map record; y is stored flipped as
 * 400 - y) and marks the markers changed; flag 0x20 clears the
 * selection. */
void GenGUIHelp(ef_addr32 widget)
{
    int16_t x;
    int16_t y;
    uint8_t flags;
    uint32_t entry;

    if (g_runtime_input_byte_c != 0u)
        return;
    x = g_gui_cursor_x;
    y = g_gui_cursor_y;
    if (x < 12)
        x = 12;
    if (y < 12)
        y = 12;
    if (x > 0x183)
        x = 0x183;
    if (y > 0x183)
        y = 0x183;
    flags = EF_U8(widget + 0xa7u);
    if ((flags & 0x10u) != 0u)
        g_selected_map_marker = (int16_t)hit_test_markers_45a54((uint32_t)(int32_t)x, (uint32_t)(int32_t)y);
    if ((flags & 0x08u) != 0u && g_selected_map_marker != -1) {
        entry = (uint32_t)((int32_t)g_selected_map_marker * 25);
        EF_U32(EF_U32(g_current_map_record + 0x20u) + entry) = (uint32_t)(int32_t)x;
        EF_U32(EF_U32(g_current_map_record + 0x20u) + entry + 4u) = (uint32_t)(0x190 - (int32_t)y);
        g_map_markers_changed = 1u;
    }
    if ((flags & 0x20u) != 0u)
        g_selected_map_marker = -1;
}

/* Target-list entries of 0x4bcd4: widget name, target type, and the
 * values stored to the full-coverage and ship-class bytes (-1 = keep). */
struct target_list_entry {
    ef_addr32 name;
    int16_t type;
    int8_t full_coverage;
    int8_t ship_class;
};

static const struct target_list_entry target_list_entries[] = {
    { 0x00184a34u, 0x00, -1, -1 },  /* "ewr_site" */
    { 0x00184b8cu, 0x13, 1, -1 },   /* "choke_point" */
    { 0x00184b98u, 0x1d, -1, -1 },  /* "pol_indust_site" */
    { 0x00184ba8u, 0x26, -1, -1 },  /* "pol_site" */
    { 0x00184bb4u, 0x31, -1, -1 },  /* "airfield" */
    { 0x00184bc0u, 0x39, -1, -1 },  /* "air_defence_site" */
    { 0x00184bd4u, 0x49, -1, -1 },  /* "port" */
    { 0x00184bdcu, 0x17, -1, 4 },   /* "kill_ship" */
    { 0x00184be8u, 0x0e, -1, 3 },   /* "supply_ship" */
    { 0x00184bf4u, 0x16, -1, -1 },  /* "carrier" */
    { 0x00184bfcu, 0x4f, 1, -1 },   /* "dam" */
    { 0x00184c00u, 0x0f, -1, 1 },   /* "awacs" */
    { 0x00184c08u, 0x10, -1, 2 },   /* "refueller" */
};

/* 0x4bcd4, 434 bytes, selection callback of the mission-planner target
 * list. Mark the list changed, clear the full-coverage and ship-class
 * bytes, then apply every entry whose name equals WIDGET's (+0x3f). */
void mpTargetListSelected(ef_addr32 widget)
{
    size_t i;

    g_target_list_changed = 1u;
    g_sam_full_coverage = 0u;
    g_target_ship_class = 0u;
    for (i = 0; i < sizeof(target_list_entries) / sizeof(target_list_entries[0]); ++i) {
        const struct target_list_entry *entry = &target_list_entries[i];

        if (guest_strcmp(widget + 0x3fu, entry->name) != 0)
            continue;
        g_target_type = entry->type;
        if (entry->full_coverage >= 0)
            g_sam_full_coverage = (uint8_t)entry->full_coverage;
        if (entry->ship_class >= 0)
            g_target_ship_class = (uint8_t)entry->ship_class;
    }
}

/* Planner state used by mpNextAssign. */
#define PLANNER_MODE           0x001a53dcu /* 3 assignment, 5 tactical target */
#define PLANNER_PANEL_X        0x0041eee6u
#define PLANNER_PANEL_Y        0x0041eee8u
#define PLANNER_TARGET_KIND    0x001a559au
#define PLANNER_TACTICAL_RECORD 0x001a5506u
#define PLANNER_TACTICAL_ENTRY 0x001a5533u
#define PLANNER_TACTICAL_PAGE  0x001a550eu
#define PLANNER_ASSIGNMENT_PAGE 0x001a5542u
#define CAMERA_YAW             0x0019e5a7u



/* Position of entry INDEX (21-byte entries at dword +0x2c) of RECORD, as
 * the three high words of the 16.16 values at +5, +7 and +9. */
static void planner_entry_position(ef_addr32 record, uint32_t index,
	int32_t *x, int32_t *height, int32_t *y)
{
	ef_addr32 entry = EF_U32(record + 0x2cu) + index * 21u;
	*x = EF_S16(entry + 7u);
	*y = EF_S16(entry + 9u);
	*height = EF_S16(entry + 11u);
}

/* Heading of RECORD in degrees (0, 90, 180 or 270 turn the view): word +0x11
 * of its entry table, which lies inside the first 21-byte entry. */
static uint16_t planner_entry_heading(ef_addr32 record)
{
	return EF_U16(EF_U32(record + 0x2cu) + 0x11u);
}

/* Bracket corners drawn around the assignment preview box. */
static void draw_preview_corners(int32_t x, int32_t y)
{
	int32_t top = y + 0xaa, bottom = y + 0xd2;
	int32_t left = x + 0xbe, right = x + 0xe6;

	draw_tactical_line(top, x + 0xc3, top, left, 0xf);
	draw_tactical_line(top, right, top, x + 0xe1, 0xf);
	draw_tactical_line(y + 0xaf, left, top, left, 0xf);
	draw_tactical_line(y + 0xaf, right, top, right, 0xf);
	draw_tactical_line(y + 0xcd, left, bottom, left, 0xf);
	draw_tactical_line(y + 0xcd, right, bottom, right, 0xf);
	draw_tactical_line(bottom, x + 0xc3, bottom, left, 0xf);
	draw_tactical_line(bottom, right, bottom, x + 0xe1, 0xf);
}

/* 0x4d2fc, 753 bytes. Render the 3D preview of the selected assignment
 * waypoint (mode 3) or tactical target entry (mode 5): the camera focus is
 * the entry position turned by the record heading (multiples
 * of 90 degrees), the yaw word steps by 0x190, and the page is drawn with
 * the record's zoom page. An assignment without a record shows the target
 * kind's default view, 50 units lower for kinds 1 and 2. In mode 3 the
 * preview box gets its corner brackets. */
void mpNextAssign(void)
{
	int32_t panel_x = EF_S16(PLANNER_PANEL_X);
	int32_t panel_y = EF_S16(PLANNER_PANEL_Y);
	int32_t x = 0, y = 0, height = 0, t;
	uint16_t heading = 0;
	int16_t page = 0;
	uint8_t kind;

	reset_view_c4105();
	if (EF_U8(PLANNER_MODE) != 3u && EF_U8(PLANNER_MODE) != 5u)
		return;
	if (EF_U8(PLANNER_MODE) == 3u && g_assignment_selected_record == 0u
		&& g_assignment_target_record == 0u)
		return;
	if (EF_U8(PLANNER_MODE) == 5u && EF_U32(PLANNER_TACTICAL_RECORD) == 0u)
		return;

	if (EF_U8(PLANNER_MODE) == 3u) {
		ef_addr32 record = g_assignment_selected_record;
		if (record != 0u) {
			planner_entry_position(record, g_assignment_current_byte,
				&x, &height, &y);
			heading = planner_entry_heading(record);
		} else {
			kind = EF_U8(PLANNER_TARGET_KIND);
			height = (kind == 1u || kind == 2u) ? -50 : 0;
		}
		page = EF_S16(PLANNER_ASSIGNMENT_PAGE);
	}
	if (EF_U8(PLANNER_MODE) == 5u) {
		ef_addr32 record = EF_U32(PLANNER_TACTICAL_RECORD);
		planner_entry_position(record, EF_U8(PLANNER_TACTICAL_ENTRY),
			&x, &height, &y);
		heading = planner_entry_heading(record);
		page = EF_S16(PLANNER_TACTICAL_PAGE);
	}

	switch (heading) {
	case 90:
		t = x; x = -y; y = t;
		break;
	case 180:
		x = -x; y = -y;
		break;
	case 270:
		t = x; x = y; y = -t;
		break;
	}
	g_key_view_focus[0] = x << 8;
	g_key_view_focus[1] = height << 8;
	g_key_view_focus[2] = y << 8;
	EF_U16(CAMERA_YAW) = (uint16_t)(EF_U16(CAMERA_YAW) + 0x190u);
	guest_fn_3122c();

	kind = EF_U8(PLANNER_TARGET_KIND);
	render_page_9e0c4((uint32_t)(int32_t)page, 0,
		((kind == 2u || kind == 1u) && EF_U8(PLANNER_MODE) == 3u) ? 0xffce0000u : 0u, 0);

	if (g_assignment_selected_record == 0u && EF_U8(PLANNER_MODE) == 3u
		&& EF_U8(PLANNER_TARGET_KIND) == 3u)
		set_ranges_4d5f0();
	renderer_finish_queue_c4167();
	if (EF_U8(PLANNER_MODE) == 3u)
		draw_preview_corners(panel_x, panel_y);
}

/* Waypoint height graph of the planner. */
#define SPRITE_SHEET           0x00126570u /* dword: 640-byte-wide sprite sheet */
#define WP_HEIGHT_ZOOM         0x001a55a1u /* 0 = 500 units per pixel, else 10 */
#define WP_HEIGHTS             0x001a560au /* int32 per waypoint */
#define WP_TYPES               0x001a5672u /* byte per waypoint */
#define WP_TYPE_END            0x11u
#define WP_TYPE_WIDE_ICON      8u /* 28x20 icon */
#define WP_TYPE_SQUARE_ICON    5u /* 21x21 icon */
/* Sprite positions, (x, y) word pairs on the sheet. */
#define SPRITE_HEIGHT_DIGITS   0x00134715u
#define SPRITE_HEIGHT_DIGITS_ZOOMED 0x0013471eu
#define SPRITE_WIDE_ICON       0x0013470cu
#define SPRITE_SQUARE_ICON     0x00134739u
#define SPRITE_WP_NUMBERS      0x001345adu
#define SHEET_STRIDE           0x280u
#define GRAPH_FLOOR            120

/* Address of the sprite at the (x, y) pair POS, DX bytes further right. */
static ef_addr32 sheet_sprite(ef_addr32 pos, int32_t dx)
{
	return EF_U32(SPRITE_SHEET) + EF_S16(pos + 2u) * SHEET_STRIDE + EF_S16(pos) + dx;
}

/* Graph row of waypoint I's height. */
static int32_t waypoint_graph_y(int16_t i, int16_t scale)
{
	return GRAPH_FLOOR - EF_S32(WP_HEIGHTS + 4u * i) / scale;
}

/* 0x4e80c, 875 bytes. Draw the waypoint height graph: the grid (26
 * columns of 20 pixels, 7 rows), the height labels on both sides and in
 * the middle, the profile from waypoint to waypoint down to the floor
 * after the last one, and per waypoint its type 8 or type 5 icon and its
 * number. The height scale is 500 units per pixel, or 10 when zoomed. */
void mpClickWPHeight(void)
{
	int16_t scale = EF_U8(WP_HEIGHT_ZOOM) == 0u ? 500 : 10;
	int16_t i, x, y;

	draw_active_marker_e5088();
	for (i = 0; i < 26; ++i)
		draw_scaled_rect_c95ac(10, 20 * i, 0, 20 * i, GRAPH_FLOOR);
	for (i = 0; i < 7; ++i)
		draw_scaled_rect_c95ac(10, 0, 20 * i, 500, 20 * i);

	/* Six labels from the bottom up; label I is column I - 1 of the
	 * digit strip, so the first starts 20 bytes before it. */
	for (i = 0, y = 118; i < 6; ++i, y -= 20) {
		ef_addr32 digits = sheet_sprite(EF_U8(WP_HEIGHT_ZOOM) == 0u
			? SPRITE_HEIGHT_DIGITS : SPRITE_HEIGHT_DIGITS_ZOOMED, (i - 1) * 20);
		guest_fn_ca620((int32_t)digits, 5, y, 12, 6, SHEET_STRIDE);
		guest_fn_ca620((int32_t)digits, 0xf5, y, 12, 6, SHEET_STRIDE);
		guest_fn_ca620((int32_t)digits, 0x1e5, y, 12, 6, SHEET_STRIDE);
	}

	for (i = 0; i < 26; ++i) {
		if (EF_U8(WP_TYPES + 1u + i) == WP_TYPE_END)
			break;
		draw_scaled_rect_c95ac(1, 20 * i, (int16_t)waypoint_graph_y(i, scale),
			20 * (i + 1), (int16_t)waypoint_graph_y(i + 1, scale));
	}
	draw_scaled_rect_c95ac(1, 20 * i, (int16_t)waypoint_graph_y(i, scale),
		(int16_t)(20 * (i + 1)), GRAPH_FLOOR);

	for (i = 0, x = -10; i < 26; ++i, x += 20) {
		uint8_t type = EF_U8(WP_TYPES + i);
		if (type == WP_TYPE_END)
			return;
		y = (int16_t)(waypoint_graph_y(i, scale) - 7);
		if (y < 3)
			y = 3;
		if (type == WP_TYPE_WIDE_ICON)
			guest_fn_ca620((int32_t)sheet_sprite(SPRITE_WIDE_ICON, 0),
				(int16_t)(20 * i - 14), (int16_t)(y - 5), 28, 20, SHEET_STRIDE);
		else if (type == WP_TYPE_SQUARE_ICON)
			guest_fn_ca620((int32_t)sheet_sprite(SPRITE_SQUARE_ICON, 0),
				(int16_t)(20 * i - 10), (int16_t)(y - 5), 21, 21, SHEET_STRIDE);
		guest_fn_ca620((int32_t)sheet_sprite(SPRITE_WP_NUMBERS, 20 * i),
			x, y, 20, 13, SHEET_STRIDE);
	}
}

/* Planner window setup. */
#define PLANNER_SLOT_SOURCE    0x00197500u /* 12-byte entries, first 8 bytes copied */
#define PLANNER_FLIGHT_NAMES   0x001a53e3u /* 16 names of 17 bytes */
#define PLANNER_FLIGHT_IDS     0x001368bcu /* dword per flight */
#define PLANNER_SCALE          0x001a53dfu /* float */
#define PLANNER_WEATHER        0x001a570au /* 10 bytes, 0..2 */

/* Fill flight name I with FORMAT (0x184a7c) and the number NUMBER. */
static void name_spare_flight(int16_t i, int32_t number)
{
	guest_sprintf_a4342(PLANNER_FLIGHT_NAMES + 17u * i, 0x00184a7cu, /* "%s %d" */
		0x0013223bu, number);
}

/* Reset the planner selection, route and waypoint state. */
static void reset_planner_state(void)
{
	int16_t i;

	EF_U8(0x001a53deu) = 0;
	g_target_type = 0;
	EF_U8(0x001a5709u) = 0;
	g_route_speed = 0x1e0;
	g_assignment_current_byte = 0;
	g_target_list_changed = 1;
	EF_U8(PLANNER_MODE) = 1;
	g_map_mode = 0;
	EF_U8(0x001a559du) = 0;
	EF_U8(0x001a559eu) = 0;
	EF_U8(WP_HEIGHT_ZOOM) = 0;
	EF_U32(0x001a56fcu) = 0;
	EF_U32(0x001a56f8u) = 0;
	EF_U8(0x001a5530u) = 0;
	g_assignment_entry_count = 0;
	g_route_selected_slot = 0;
	EF_U16(0x001a56f6u) = 0;
	g_assignment_target_record = 0;
	g_assignment_selected_record = 0;
	EF_U32(PLANNER_TACTICAL_RECORD) = 0;
	EF_U32(0x001a550au) = 0;
	g_tactical_reset_mode = 0;
	EF_U8(0x001a553au) = 1;
	EF_U8(PLANNER_TACTICAL_ENTRY) = 0;
	g_tactical_status_changed = 0;
	g_route_side = 1;

	for (i = 0; i < 26; ++i) {
		EF_U8(WP_TYPES + i) = WP_TYPE_END;
		EF_S32(WP_HEIGHTS + 4u * i) = 1000;
	}
	EF_S32(WP_HEIGHTS) = 0;
	EF_S32(WP_HEIGHTS + 4u * 25u) = 0;
	EF_U8(0x001a5535u) = 0;
	for (i = 0; i < 4; ++i) {
		EF_U8(0x001a5536u + i) = 0;
		EF_U8(0x001a553cu + i) = 0;
	}
}

/* 0x4b1f0, 1199 bytes. Open the mission planner unless it is already
 * open: copy the scene slots, look up the zoom pages, mirror the three
 * map layer flags, create the 480x400 map picture and show the planner
 * widgets, set the map scale, reset the planner state, name the 16
 * flight buttons (the roster ids, then numbered spares; without a roster
 * the first is the 7-byte name 0x184a84), seed the ten weather bytes with
 * a random walk over 0..2 and show the preview picture. */
void mpSetWindows(void)
{
	ef_addr32 widget, list;
	uint32_t picture;
	int16_t i, count, value;
	char name[20];

	if (g_frontend_network_active != 0u)
		return;

	for (i = 0; i < EF_S16(g_current_map_record + 0x1cu); ++i) {
		ef_addr32 scene = g_current_map_record;
		ef_addr32 slot = EF_U32(scene + 0x20u) + 25u * i;
		EF_U32(slot) = EF_U32(PLANNER_SLOT_SOURCE + 12u * i);
		EF_U32(slot + 4u) = EF_U32(PLANNER_SLOT_SOURCE + 12u * i + 4u);
	}
	EF_U16(0x001a53d2u) = (uint16_t)call_9de08_9ddfc(0x001849c8u); /* "sea" */
	EF_U16(0x001a53d6u) = (uint16_t)call_9de08_9ddfc(0x001849ccu); /* "type42" */
	EF_U16(0x001a53d4u) = (uint16_t)call_9de08_9ddfc(0x001849d4u); /* "oilsmall" */
	EF_U16(0x001a53d0u) = (uint16_t)call_9de08_9ddfc(0x001849d4u); /* "oilsmall" */
	EF_U8(0x001a53d8u) = (uint8_t)g_map_show_nato;
	EF_U8(0x001a53dau) = (uint8_t)g_map_show_neutral;
	EF_U8(0x001a53d9u) = (uint8_t)g_map_show_russian;

	picture = alloc_picture_c3670(0x001849e0u, 0, 0, 0x1e0, 0x190); /* "missplan.lbm" */
	set_widget_params_a5b1c(find_named_or_abort_a46cc(0x001849f0u), picture, /* "mp_target_view" */
		0x1e0, 0x190, 0x1e0);
	activate_widget_a4cb4(EF_U8(find_named_or_abort_a46cc(0x00184a00u) + 0x10u)); /* "campaign" */
	widget = find_named_or_abort_a46cc(0x00184a0cu); /* "mp_options" */
	activate_widget_a4cb4(EF_U8(widget + 0x10u));
	enable_widget_a72d8(find_widget_checked_a78bc(widget, 0x00184a18u)); /* "mp_strike" */
	widget = find_named_or_abort_a46cc(0x00184a24u); /* "mp_strike_list" */
	activate_widget_a4cb4(EF_U8(widget + 0x10u));
	show_named_widget_a7318(widget, 0x00184a34u); /* "ewr_site" */
	widget = find_named_or_abort_a46cc(0x00184a40u); /* "mp_map" */
	activate_widget_a4cb4(EF_U8(widget + 0x10u));
	*(float *)ef_guest_memory_ptr(PLANNER_SCALE) = (float)((*(double *)ef_guest_memory_ptr(widget + 0x9fu) + *(double *)ef_guest_memory_ptr(0x00184ab0u))
		* *(double *)ef_guest_memory_ptr(0x00184ab8u) + 1.0);
	activate_widget_a4cb4(EF_U8(find_named_or_abort_a46cc(0x00184a48u) + 0x10u)); /* "mp_map_options" */
	activate_widget_a4cb4(EF_U8(find_named_or_abort_a46cc(0x00184a58u) + 0x10u)); /* "mp_strike_info" */
	clear_children_dirty(find_named_or_abort_a46cc(0x00184a68u)); /* "mp_wp_set" */

	reset_planner_state();

	if (g_tactical_lookup_count > 1) {
		for (i = 0; i < g_tactical_lookup_count; ++i)
			guest_sprintf_a4342(PLANNER_FLIGHT_NAMES + 17u * i, 0x00184a74u, /* "%.10s" */
				EF_U32(PLANNER_FLIGHT_IDS + 4u * i));
		for (i = g_tactical_lookup_count; i < 16; ++i)
			name_spare_flight(i, i - g_tactical_lookup_count + 1);
	} else {
		for (i = 0; i < 7; ++i)
			EF_U8(PLANNER_FLIGHT_NAMES + i) = EF_U8(0x00184a84u + i); /* "Player" */
		for (i = 1; i < 16; ++i)
			name_spare_flight(i, i);
	}
	list = find_named_or_abort_a46cc(0x00184a8cu); /* "mp_base_flights" */
	for (i = 0; i < 16; ++i) {
		guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x00184a9cu, (int32_t)i); /* "name%d" */
		show_widget_a7588(find_widget_checked_a78bc(list, (ef_addr32)(uintptr_t)name),
			PLANNER_FLIGHT_NAMES + 17u * i);
		g_tactical_slots[i] = 0xff;
	}
	for (i = 0; i < 16; ++i)
		EF_U8(0x001a5510u + i) = 0;

	value = (int16_t)((uint16_t)half_word_9c960() % 3u);
	for (i = 0; i < 10; ++i) {
		EF_U8(PLANNER_WEATHER + i) = (uint8_t)value;
		value = (int16_t)(value + (int32_t)((uint16_t)half_word_9c960() % 3u) - 1);
		if (value < 0)
			value = 0;
		if (value > 2)
			value = 2;
	}
	EF_U16(0x0019e56cu) = 0;
	EF_U16(0x0019e56eu) = 0;
	EF_U16(0x0019e570u) = 0;
	EF_U8(0x0019e5dfu) = 0;
	EF_U8(0x0019e5e3u) = 1;
	show_preview_picture_41314(0x00184aa4u); /* "norgeog.lbm" */
}

