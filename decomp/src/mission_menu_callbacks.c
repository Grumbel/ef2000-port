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
 * EAX, which the GUI dispatcher sees, is the roster count on the early
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
