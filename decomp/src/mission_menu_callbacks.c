/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

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

/* 0x50c40, 43 bytes: invert + update target view label. */
void mp_redraw_target_view_50c40(ef_addr32 widget)
{
    uint8_t mode = (uint8_t)(1u - EF_U8(0x1a53deu));
    EF_U8(0x1a53deu) = mode;
    show_widget_a7588(widget, mode == 0u ? 0x13211eu : 0x132119u);
}

#ifdef EF_DECOMP_HOST_TEST

/* 0x4ec44, 44 bytes: the Mission1Click body subtracts 60 from the
 * unsigned 16-bit speed (wrapping) then clamps only small values. */
void mp_mission_1_click_4ec44(void)
{
    uint16_t speed = (uint16_t)(EF_U16(0x1a56f4u) - 60u);
    EF_U16(0x1a56f4u) = speed < 120u ? 120u : speed;
    mp_ui_guest_call_084(0x4ec70u, 0u, 0u);
}

/* 0x50c2c, 18 bytes: tail-jump to map zoom or alternative guest mode. */
void mp_change_mode_50c2c(ef_addr32 widget)
{
    if (EF_U8(0x1a53deu) == 0u)
        mp_zoom_in_map_4bc40(widget);
    else
        mp_ui_guest_call_084(0xa870u, widget, 0u);
}

#endif
