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
