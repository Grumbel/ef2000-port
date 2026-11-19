/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x4ec44, 44 bytes: the Mission1Click body subtracts 60 from the
 * unsigned 16-bit speed (wrapping) then clamps only small values. */
void mp_mission_1_click_4ec44(void)
{
    uint16_t speed = (uint16_t)(EF_U16(0x1a56f4u) - 60u);
    EF_U16(0x1a56f4u) = speed;
    if (speed < 120u)
        EF_U16(0x1a56f4u) = 120u;
    rebuild_waypoint_speed_4ec70();
}

static void close_warning(ef_addr32 identifier)
{
    ef_addr32 window = find_named_or_abort_a46cc(identifier);
    close_widget_a4b80(EF_U8(window + 0x10u));
    reset_view_ac9e8();
}

/* 0x4f2f8, 28 bytes: dialog OK selects its widget's +0x10 byte. */
void mp_click_v8_okay_4f2f8(void)
{
    close_warning(0x184b80u);
}

/* 0x4f36c, 28 bytes: warning dialog follows identical sequence. */
void mp_warning_ok_click_4f36c(void)
{
    close_warning(0x184e18u);
}

/* 0x505e4, 32 bytes: close and release campaign dialog in order. */
void mp_return_to_campaign_505e4(void)
{
    ef_addr32 window = find_named_or_abort_a46cc(0x184ec4u);
    apply_a4cb4_a4d90(0x184ec4u);
    redraw_widget_ac9a0(window);
}

/* 0x50604, 52 bytes: restore three saved menu state bytes and leave. */
void mp_no_quit_50604(void)
{
    EF_U16(0x126554u) = EF_U8(0x1a53d8u);
    EF_U16(0x126556u) = EF_U8(0x1a53d9u);
    EF_U16(0x126558u) = EF_U8(0x1a53dau);
    reset_view_ac9e8();
    EF_U8(0x1a53dcu) = 6u;
}

/* 0x50750, 32 bytes: task toggle is zero-vs-nonzero, not XOR 1. */
void mp_miss_plan_task_50750(void)
{
    uint8_t task = EF_U8(0x1a553au);

    EF_U8(0x12654cu) = 1u;
    EF_U8(0x1a54f3u) = 1u;
    EF_U8(0x1a553au) = task == 0u;
}

static uint8_t menu_widget_id(ef_addr32 name)
{
    ef_addr32 item = find_named_or_abort_a46cc(name);
    return EF_U8(item + 0x10u);
}

/* 0x507f4, 23 bytes: set role by selected widget's choice byte. */
void mp_set_role_507f4(void)
{
    close_widget_a4b80(menu_widget_id(0x184eccu));
}

/* 0x5080c, 46 bytes: select aircraft type, deselect previous selection. */
void mp_set_ac_type_5080c(void)
{
    close_widget_a4b80(menu_widget_id(0x184af0u));
    activate_widget_a4cb4(menu_widget_id(0x184a8cu));
}

/* 0x50c2c, 18 bytes: tail-jump to map zoom or the high-score callback. */
void mp_change_mode_50c2c(ef_addr32 widget)
{
    if (EF_U8(0x1a53deu) == 0u)
        mp_zoom_in_map_4bc40(widget);
    else
        open_hiscores_a870(widget);
}

/* 0x50c40, 43 bytes: invert + update target view label. */
void mp_redraw_target_view_50c40(ef_addr32 widget)
{
    uint8_t mode = (uint8_t)(1u - EF_U8(0x1a53deu));
    EF_U8(0x1a53deu) = mode;
    show_widget_a7588(widget, mode == 0u ? 0x13211eu : 0x132119u);
}

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
