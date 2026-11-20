/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Mission-planning (gpw) window callbacks from the GUI table at 0x12b4c8.
 * They use the plain Watcom register ABI; dialog and widget lookups abort
 * inside the callee when a name is missing. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x69f1c, 141 bytes, callback slot 0x12b680. Save the 20 three-byte
 * pair marks, clear the GUI word 0x41eeda and mark the plane window
 * redrawn. In connection mode 3 refresh the slot list first. When the
 * socket is ready, refresh again in mode 1 and request "txmiss";
 * otherwise set the pending latch to 100. Mode 3 finally shows the
 * "loading" widget of "loadingwin2" (0xa46cc keeps EDX, so 0xa78bc
 * receives the widget name loaded before the dialog lookup). EBX and
 * EDX are saved. EAX returns the connection-mode byte, or in mode 3
 * whatever the widget show leaves there, as the original does. The GUI
 * dispatch sites (0xa4bce, 0xa4d01, 0xa5af7, 0xa5c84, 0xa6166) discard
 * it. */
uint32_t gpwRedrawPlaneWindow(void)
{
    memcpy_dc832((ef_addr32)(uintptr_t)g_saved_object_pair_marks,
                 (ef_addr32)(uintptr_t)g_object_pair_marks,
                 sizeof g_saved_object_pair_marks);
    clear_word_41eeda();
    g_plane_window_redrawn = 1u;
    if (g_frontend_connection_mode == 3u)
        refresh_slot_list_69d10();
    if (g_socket_pending_ready != 0u) {
        if (g_frontend_connection_mode == 1u)
            refresh_slot_list_69d10();
        set_pending_name_ac7a8(0x00186354u);            /* "txmiss" */
    } else {
        g_frontend_pending_latch = 100u;
    }
    if (g_frontend_connection_mode != 3u)
        return g_frontend_connection_mode;
    return show_widget_a7588_r(find_widget_checked_a78bc(
                                   find_named_or_abort_a46cc(0x0018635cu), /* "loadingwin2" */
                                   0x00186368u),                      /* "loading" */
                               0x00131102u);
}

/* 0x7cad4, 153 bytes, open callback of the current-mission dialog ROOT.
 * Enable the ground-effects widget for the current setting ("geffecty"
 * or "geffectn"), the difficulty widget ("high" for a non-zero level,
 * else "low") and the "defweapyes" and "revweapno" widgets. Unless the
 * option word is set while the socket is not ready, set bit 6 of the
 * "defweapno" and "revweapyes" widgets. EAX returns what the last callee
 * leaves there, as the original does. */
uint32_t onopenCurrentMission(ef_addr32 root)
{
    uint32_t result;

    show_named_widget_a7318(root, g_ground_effects_enabled != 0u ? 0x00187f44u  /* "geffecty" */
                                                                 : 0x00187f50u); /* "geffectn" */
    show_named_widget_a7318(root, g_mission_difficulty != 0u ? 0x00187eecu  /* "high" */
                                                             : 0x00187ee8u); /* "low" */
    show_named_widget_a7318(root, 0x00187f10u);          /* "defweapyes" */
    result = show_named_widget_a7318(root, 0x00187f5cu); /* "revweapno" */
    if (g_mission_option_word == 0u || g_socket_pending_ready != 0u) {
        set_widget_bit6_r(find_widget_checked_a78bc(root, 0x00187f68u), 1u); /* "defweapno" */
        result = set_widget_bit6_r(find_widget_checked_a78bc(root, 0x00187f04u), 1u); /* "revweapyes" */
    }
    return result;
}

static ef_addr32 params_widget(ef_addr32 name)
{
    return find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187e90u), name); /* "params" */
}

/* 0x7cb70, 199 bytes, click callback of the weapon-choice buttons on
 * the "params" dialog (WIDGET name text at +0x3f). With the option word
 * clear, or once the socket is ready, enable "revweapno" and
 * "defweapyes". Otherwise "defweapno" enables "revweapyes" and clears
 * "revweapno", and "revweapno" enables "defweapyes" and clears
 * "defweapno": the two choices exclude each other. */
void gpwTurnXPlane(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;

    if (g_mission_option_word == 0u || g_socket_pending_ready != 0u) {
        enable_widget_a72d8(params_widget(0x00187f5cu)); /* "revweapno" */
        enable_widget_a72d8(params_widget(0x00187f10u)); /* "defweapyes" */
        return;
    }
    if (guest_strcmp(name, 0x00187f68u) == 0) {      /* "defweapno" */
        enable_widget_a72d8(params_widget(0x00187f04u)); /* "revweapyes" */
        clear_widget_dirty(params_widget(0x00187f5cu));  /* "revweapno" */
    } else if (guest_strcmp(name, 0x00187f5cu) == 0) { /* "revweapno" */
        enable_widget_a72d8(params_widget(0x00187f10u)); /* "defweapyes" */
        clear_widget_dirty(params_widget(0x00187f68u));  /* "defweapno" */
    }
}

/* 0x7d138, 184 bytes, redraw callback of the enemy-view buttons; the
 * WIDGET name (+0x3f) is a decimal slot number. A slot without a unit
 * type shows the generic aircraft "efa_glob" with detail 3500. A typed
 * slot looks up the type's name and its scene word, which defaults to
 * 50. When the type has page text, page text 7 of the looked-up page is
 * run. The slots are then synchronized (simSyncSlots). The number is
 * parsed again before each table read, as in the original. */
void RedrawEnemyView(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    const ef_unit_type_record *type;

    if (g_enemy_view_types[guest_convert_parser_integer(name)] == 0) {
        g_enemy_view_page = (int16_t)guest_lookup_value(0x00188028u); /* "efa_glob" */
        g_enemy_view_detail = 0x0dac;
    } else {
        type = &g_unit_type_records[g_enemy_view_types[guest_convert_parser_integer(name)]];
        g_enemy_view_page = (int16_t)guest_lookup_value(type->name);
        g_enemy_view_detail = scene_lookup_named_word(g_enemy_view_page);
        if (g_enemy_view_detail == 0)
            g_enemy_view_detail = 50;
    }
    type = &g_unit_type_records[g_enemy_view_types[guest_convert_parser_integer(name)]];
    if (type->has_page_text != 0u)
        run_page_text_9e508(g_enemy_view_page, 7u);
    simSyncSlots(widget);
}

/* 0xb5d0, 204 bytes, redraw callback for the dialog named NAME. While a
 * target group is selected (index word 0x1976a0 not -1, window word
 * 0x19769a set, group list non-empty) and NAME is "targview", walk to
 * the selected group; when its record has a member entry at the selected
 * member index (124-byte members, dword +0x51), activate and refresh the
 * "targetwin" and "targetzoom" dialogs. */
void gpwRedrawTargetWindow(ef_addr32 name)
{
    ef_addr32 node;
    ef_addr32 dialog;
    uint32_t i = 0;

    if (g_target_group_index == -1 || g_target_window_open == 0 || g_group_list_head == 0u)
        return;
    if (guest_strcmp(name, 0x0018083cu) != 0)                  /* "targview" */
        return;
    node = g_group_list_head;
    while (node != 0u && (int16_t)i < g_target_group_index) {
        node = EF_U32(node + 4u);
        ++i;
    }
    if (node == 0u)
        return;
    if (EF_U32(EF_U32(node + 0x13u) + 0x51u + (uint32_t)((int32_t)g_target_member_index * 124)) == 0u)
        return;
    dialog = find_named_or_abort_a46cc(0x001805f0u);           /* "targetwin" */
    activate_widget_a4cb4(EF_U8(dialog + 0x10u));
    refresh_flagged_object_a4b08(dialog);
    dialog = find_named_or_abort_a46cc(0x001805fcu);           /* "targetzoom" */
    activate_widget_a4cb4(EF_U8(dialog + 0x10u));
    refresh_flagged_object_a4b08(dialog);
}

/* 0xa5d8, 292 bytes, accept callback of the numbered campaign target
 * buttons (WIDGET name at +0x3f). The map markers' positions are first
 * restored from the 12-byte save table at 0x197500. The chosen target's
 * group and member become the selection (also mirrored to 0x125835),
 * the network value is refreshed from the group's adjacency list when
 * the frontend network is active, the target views are rebuilt, the
 * marker positions saved again and "missionoverview" redrawn. */
void gpwCampAccepted(ef_addr32 widget)
{
    const ef_campaign_target_choice *choice;
    int32_t i;

    for (i = 0; i < (int32_t)(int16_t)EF_U16(g_current_map_record + 0x1cu); ++i) {
        EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u) = g_saved_marker_positions[i].x;
        EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u + 4u) = g_saved_marker_positions[i].y;
    }
    choice = &g_campaign_target_choices[guest_convert_parser_integer(widget + 0x3fu) - 1];
    g_target_member_index = choice->member;
    g_target_group_index = choice->group;
    g_reference_mirror[2] = (uint8_t)g_target_member_index;
    g_reference_mirror[0] = (uint8_t)choice->group;
    g_reference_mirror[1] = (uint8_t)((uint16_t)choice->group >> 8);
    g_reference_mirror[3] = 0u;
    if (g_frontend_network_active != 0u) {
        g_adjacency_selected_value =
            (int16_t)EF_U16(g_grid_adjacency_base + (uint32_t)((int32_t)g_target_group_index * 17) + 1u +
                            (uint32_t)((int32_t)g_target_member_index * 2));
        guest_fn_421c0_w();
    }
    ui_emit_selected_message();
    guest_fn_9820();
    guest_fn_9ea0();
    for (i = 0; i < (int32_t)(int16_t)EF_U16(g_current_map_record + 0x1cu); ++i) {
        g_saved_marker_positions[i].x = EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u);
        g_saved_marker_positions[i].y = EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u + 4u);
    }
    redraw_item_a5ef8(EF_U8(find_named_or_abort_a46cc(0x00180584u) + 0x10u)); /* "missionoverview" */
}

/* Draw the image named by NAME (0 = none) into BUFFER and show it in
 * DIALOG at WIDTH x HEIGHT. */
static void show_weapon_image(ef_addr32 dialog, ef_addr32 name, ef_addr32 buffer,
                              uint16_t width, uint16_t height)
{
    if (name == 0u)
        return;
    guest_fn_c36b4(name, 0u, 0u, width, height, buffer);
    set_widget_params_a5b1c(dialog, buffer, width, height, width);
}

/* 0x69bdc, 305 bytes, redraw callback of the weapon list; WIDGET's name
 * (+0x3f) names the weapon. Title the "currentweapon" dialog with it,
 * select the weapon record (0x699f4), and when one is found draw its
 * small (320x200) and large (640x400) pictures into the "currentweapon"
 * and "bigcurrweapon" dialogs. The weapon's page words are assigned and
 * word 0x1282d8 set to 1. */
void gpwRedrawCurrentWeaponWindow(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    ef_addr32 title = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00186328u), /* "currentweapon" */
                                                0x0018630cu);                         /* "title" */

    ef_addr32 dialog;

    g_current_weapon = (int16_t)find_named_record_699f4(name);
    if (title != 0u)
        show_widget_a7588(title, name);
    dialog = find_named_or_abort_a46cc(0x00186328u);                        /* "currentweapon" */
    if (g_current_weapon != 0)
        show_weapon_image(dialog, g_weapon_records[g_current_weapon].small_image,
                          0x001e74a0u, 0x140u, 0xc8u);
    dialog = find_named_or_abort_a46cc(0x00186338u);                        /* "bigcurrweapon" */
    if (g_current_weapon != 0)
        show_weapon_image(dialog, g_weapon_records[g_current_weapon].large_image,
                          0x001a8ca0u, 0x280u, 0x190u);
    assign_page_words_68e84((uint32_t)(int32_t)g_current_weapon);
    g_weapon_page_flag = 1;
}

/* 0x43020, 386 bytes, close callback of the pilot selection. Unless the
 * frontend network is already active: close "menu", restore the map
 * marker positions saved at 0x197500 (byte index, as in the original),
 * reallocate the active record pool, mark the network active, open
 * "test" and "flightinfo", place the flight window (x + 8, 24 to the
 * left once it is past 23), reset the slots, world state and DOS
 * buffers, refresh the adjacency value and its views, close
 * "timeadjust" and, when the performance display is enabled, lower the
 * performance figure by 10 (to 0 below 10) and show it as "<n>%". */
void gpwClosePilots(void)
{
    char text[0x34];
    uint8_t i;
    uint8_t performance;
    ef_addr32 widget;

    if (g_frontend_network_active != 0u)
        return;
    apply_a4b80_a4db0(0x00184558u);                         /* "menu" */
    for (i = 0; (int16_t)i < (int16_t)EF_U16(g_current_map_record + 0x1cu); ++i) {
        EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u) = g_saved_marker_positions[i].x;
        EF_U32(EF_U32(g_current_map_record + 0x20u) + (uint32_t)i * 25u + 4u) = g_saved_marker_positions[i].y;
    }
    clear_word_41eeda();
    release_pointer_b1a10(0x001379bcu);
    g_active_record_pool = allocate_weighted_node_records(0x00137970u, 0x001379a8u);
    g_frontend_network_active = 1u;
    apply_a4cb4_a4d90(0x00184550u);                         /* "test" */
    apply_a4cb4_a4d90(0x001844a4u);                         /* "flightinfo" */
    g_flight_window_x = (int16_t)(g_flight_origin_x + 8);
    g_flight_window_y = g_flight_origin_y;
    if (g_flight_window_x > 0x17)
        g_flight_window_x = (int16_t)(g_flight_window_x - 0x18);
    clear_slots_41f24();
    reset_world_state_410d0();
    dos_commit_f44();
    g_adjacency_selected_value =
        (int16_t)EF_U16(g_grid_adjacency_base + (uint32_t)((int32_t)g_target_group_index * 17) + 1u +
                        (uint32_t)((int32_t)g_target_member_index * 2));
    guest_fn_421c0_w();
    apply_a4b80_a4db0(0x0018457cu);                         /* "timeadjust" */
    set_word_41eeda();
    if (g_performance_display == 0u)
        return;
    performance = g_performance_value;
    g_performance_value = (uint8_t)(performance < 10u ? 0u : performance - 10u);
    widget = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00184588u), /* "performance" */
                                       0x00184588u);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00184594u, (uint32_t)g_performance_value); /* "%d%%" */
    show_widget_a7588(widget, (ef_addr32)(uintptr_t)text);
}

/* Layer-toggle checkbox of the "mapmodeswin" dialog: bit 0 of the
 * widget's +0x1d flags. */
static int16_t map_layer_checked(ef_addr32 dialog, ef_addr32 name)
{
    return (int16_t)((EF_U8(find_widget_checked_a78bc(dialog, name) + 0x1du) & 1u) != 0u);
}

/* 0xa6fc, 371 bytes (tracker name gpwFlyMission), callback of the map
 * mode selection; WIDGET's name (+0x3f) is the chosen mode. "geographical",
 * "political" and "strategic" select map mode 0/1/2 and its background
 * picture; any other name keeps the mode. Then the nine layer flags
 * are read back from the "mapmodeswin" checkboxes and the selection
 * message is emitted. */
void gpwFlyMission(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    ef_addr32 dialog;

    if (guest_strcmp(name, 0x00180730u) == 0) {          /* "geographical" */
        show_preview_picture_41314(0x00180740u);            /* "map03.lbm" */
        g_map_mode = 0;
    } else if (guest_strcmp(name, 0x0018074cu) == 0) {   /* "political" */
        show_preview_picture_41314(0x00180758u);            /* "map04.lbm" */
        g_map_mode = 1;
    } else if (guest_strcmp(name, 0x00180764u) == 0) {   /* "strategic" */
        show_preview_picture_41314(0x00180770u);            /* "norjstr.lbm" */
        g_map_mode = 2;
    }
    dialog = find_named_or_abort_a46cc(0x0018077cu);         /* "mapmodeswin" */
    g_map_show_nato = map_layer_checked(dialog, 0x00180788u);
    g_map_show_russian = map_layer_checked(dialog, 0x00180790u);
    g_map_show_neutral = map_layer_checked(dialog, 0x00180798u);
    g_map_show_airdef = map_layer_checked(dialog, 0x001807a0u);
    g_map_show_airsup = map_layer_checked(dialog, 0x001807a8u);
    g_map_show_navy = map_layer_checked(dialog, 0x001807b0u);
    g_map_show_comms = map_layer_checked(dialog, 0x001807b8u);
    g_map_show_supply = map_layer_checked(dialog, 0x001807c0u);
    g_map_show_polit = map_layer_checked(dialog, 0x001807c8u);
    ui_emit_selected_message();
}

/* Step the selected adjacency unit by DELTA, skipping the 0xff
 * sentinel and wrapping around the unit count (16-bit arithmetic). */
static void step_selected_unit(int16_t delta)
{
    int16_t count = g_unit_count;
    int16_t unit = (int16_t)(g_adjacency_selected_value + delta);

    if (unit == 0xff)
        unit = (int16_t)(unit + (delta > 0 ? 1 : -1));
    if (delta > 0 && unit >= count)
        unit = (int16_t)(delta == 1 ? 0 : unit - count);
    else if (delta < 0 && unit < 0)
        unit = (int16_t)(delta == -1 ? count - 1 : unit + count);
    g_adjacency_selected_value = unit;
}

/* 0x431a4, 597 bytes, button callback of the flight-info window; the
 * widget's one-letter name (+0x3f) picks the action:
 *   a  clear the selection and page, close "flightinfo"
 *   b/c  show page 0x13474a / 0x134d8a
 *   d  set the info mode byte
 *   e  toggle the info toggle byte
 *   f/g/h/i  (network active only) select the next/previous unit or
 *            step by +10/-10, then refresh the views and emit the
 *            selection message. */
void gpwOnOpenTargetWindow(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    int16_t delta;

    if (guest_strcmp(name, 0x0018459cu) == 0) {          /* "a" */
        g_flightinfo_page = 0u;
        g_flightinfo_mode = 0u;
        g_adjacency_selected_value = -1;
        find_named_or_abort_a46cc(0x001844a4u);             /* "flightinfo" */
        return;
    }
    if (guest_strcmp(name, 0x001845a0u) == 0) {          /* "b" */
        g_flightinfo_page = 0x0013474au;
        return;
    }
    if (guest_strcmp(name, 0x001845a4u) == 0) {          /* "c" */
        g_flightinfo_page = 0x00134d8au;
        return;
    }
    if (guest_strcmp(name, 0x001845a8u) == 0) {          /* "d" */
        g_flightinfo_mode = 1u;
        return;
    }
    if (guest_strcmp(name, 0x001845acu) == 0) {          /* "e" */
        g_flightinfo_toggle = (uint8_t)(g_flightinfo_toggle == 0u);
        return;
    }
    if (guest_strcmp(name, 0x001845b0u) == 0)            /* "f" */
        delta = 1;
    else if (guest_strcmp(name, 0x001845b4u) == 0)       /* "g" */
        delta = -1;
    else if (guest_strcmp(name, 0x001845b8u) == 0)       /* "h" */
        delta = 10;
    else if (guest_strcmp(name, 0x001845bcu) == 0)       /* "i" */
        delta = -10;
    else
        return;
    if (g_frontend_network_active == 0u)
        return;
    step_selected_unit(delta);
    guest_fn_421c0_w();
    ui_emit_selected_message();
}

/* Size of a group member record (group record +0x13 points at them). */
#define GROUP_MEMBER_SIZE 0x7cu

/* 0x426a4, 477 bytes, update callback of the campaign map; clears bits
 * 1-2 of WIDGET +0xc4. With groups present: rebuild the view stages
 * (unless byte 0x121e8c is set) and the cell grid, then either draw
 * every group member (in flight-info mode; side 1 in colour 1, others
 * 9) or find the selected one. The selection is the selected unit's
 * group and row slot (a slot past 7 reports "Failed to find expected
 * group!!") or, with no unit, the selected target group and member. A
 * missing group or member ends the update; otherwise that member
 * becomes the current map record and is drawn, except for a target
 * selection while the network is active. In network mode the per-row
 * and per-unit updates (unit 0xff skipped) run before the map nodes are
 * rebuilt. */
void gpwUpdateMap(ef_addr32 widget)
{
    ef_addr32 node;
    ef_addr32 member;
    int16_t group;
    int16_t slot;
    int16_t i;

    EF_U8(widget + 0xc4u) &= 0xf9u;
    if (g_group_list_head != 0u) {
        if (g_runtime_input_byte_c == 0u)
            update_view_stages_45574(0x00137980u, 0x00137988u, 0x00137960u);
        node = g_group_list_head;
        draw_cell_grid_42568();
        if (g_flightinfo_mode != 0u) {
            for (; node != 0u; node = EF_U32(node + 4u)) {
                for (i = 0; i < (int16_t)EF_U8(node + 0x12u); ++i)
                    draw_segment_records_418c4(EF_U32(node + 0x13u) + (uint32_t)((int32_t)i * (int32_t)GROUP_MEMBER_SIZE),
                                               EF_U8(node + 0xbu) == 1u ? 1 : 9);
            }
        } else {
            if (g_adjacency_selected_value > -1) {
                ef_addr32 unit = g_world_grid_base + (uint32_t)((int32_t)g_adjacency_selected_value * 0xca);

                group = (int16_t)EF_U16(unit + (uint32_t)offsetof(ef_world_grid_cell, adjacency_group));
                slot = find_row_slot_821f8((uint16_t)g_adjacency_selected_value, group);
                /* ORIGINAL: EDX (the slot) is not reloaded after the message call. */
                if (slot > 7)
                    guest_callptr1_x(EF_U32(0x00132a74u), 0x001844f0u); /* "Failed to find expected group!!" */
            } else {
                group = g_target_group_index;
                slot = g_target_member_index;
            }
            for (i = 0; node != 0u && i < group; ++i)
                node = EF_U32(node + 4u);
            if (node == 0u || slot >= (int16_t)EF_U8(node + 0x12u))
                return;
            member = EF_U32(node + 0x13u) + (uint32_t)((int32_t)slot * (int32_t)GROUP_MEMBER_SIZE);
            g_current_map_record = member;
            if (g_adjacency_selected_value > -1 || g_frontend_network_active == 0u)
                draw_segment_records_418c4(member, 9);
        }
    }
    if (g_group_list_head != 0u && g_frontend_network_active != 0u) {
        for (i = 0; i < g_map_row_count; ++i)
            guest_fn_41b98(i);
        for (i = 0; i < g_unit_count;) {
            guest_fn_41998(i);
            ++i;
            if (i == 0xff)
                ++i;
        }
        guest_fn_42004();
    }
    guest_fn_4c120_x(0x00137980u, 0x00137960u);
    draw_map_nodes_4c3bc(0x00137980u, 0x00137988u);
}
