/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Mission-planning (gpw) window callbacks from the GUI table at 0x12b4c8.
 * They use the plain Watcom register ABI; dialog and widget lookups abort
 * inside the callee when a name is missing. */

#include "ef_abi.h"
#include "ef_globals.h"

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
