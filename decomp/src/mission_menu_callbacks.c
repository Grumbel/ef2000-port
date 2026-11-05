/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t guest(uint32_t target, uint32_t eax, uint32_t edx)
{
    return mp_ui_guest_call_084(target, eax, edx);
}

/* 0x4ec44, 44 bytes: the Mission1Click body subtracts 60 from the
 * unsigned 16-bit speed (wrapping) then clamps only small values. */
void mp_mission_1_click_4ec44(void)
{
    uint16_t speed = (uint16_t)(EF_U16(0x1a56f4u) - 60u);
    EF_U16(0x1a56f4u) = speed < 120u ? 120u : speed;
    guest(0x4ec70u, 0u, 0u);
}

static void close_warning(uint32_t identifier)
{
    uint32_t window = guest(0xa46ccu, identifier, 0u);
    guest(0xa4b80u, EF_U8(window + 0x10u), 0u);
    guest(0xac9e8u, 0u, 0u);
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
    uint32_t window = guest(0xa46ccu, 0x184ec4u, 0u);
    guest(0xa4d90u, 0x184ec4u, 0u);
    guest(0xac9a0u, window, 0u);
}

/* 0x50604, 52 bytes: restore three saved menu state bytes and leave. */
void mp_no_quit_50604(void)
{
    EF_U16(0x126554u) = EF_U8(0x1a53d8u);
    EF_U16(0x126556u) = EF_U8(0x1a53d9u);
    EF_U16(0x126558u) = EF_U8(0x1a53dau);
    guest(0xac9e8u, 0u, 0u);
    EF_U8(0x1a53dcu) = 6u;
}

/* 0x50750, 32 bytes: task toggle is zero-vs-nonzero, not XOR 1. */
void mp_miss_plan_task_50750(void)
{
    EF_U8(0x12654cu) = 1u;
    EF_U8(0x1a54f3u) = 1u;
    EF_U8(0x1a553au) = EF_U8(0x1a553au) == 0u ? 1u : 0u;
}

static uint8_t menu_item_type(uint32_t name)
{
    uint32_t item = guest(0xa46ccu, name, 0u);
    return EF_U8(item + 0x10u);
}

/* 0x507f4, 23 bytes: set role by selected widget's choice byte. */
void mp_set_role_507f4(void)
{
    guest(0xa4b80u, menu_item_type(0x184eccu), 0u);
}

/* 0x5080c, 46 bytes: select aircraft type, deselect previous selection. */
void mp_set_ac_type_5080c(void)
{
    guest(0xa4b80u, menu_item_type(0x184af0u), 0u);
    guest(0xa4cb4u, menu_item_type(0x184a8cu), 0u);
}

/* 0x50c2c, 18 bytes: tail-jump to map zoom or alternative guest mode. */
void mp_change_mode_50c2c(ef_addr32 widget)
{
    if (EF_U8(0x1a53deu) == 0u)
        mp_zoom_in_map_4bc40(widget);
    else
        guest(0xa870u, widget, 0u);
}

/* 0x50c40, 43 bytes: invert + update target view label. */
void mp_redraw_target_view_50c40(ef_addr32 widget)
{
    uint8_t mode = (uint8_t)(1u - EF_U8(0x1a53deu));
    EF_U8(0x1a53deu) = mode;
    guest(0xa7588u, widget, mode == 0u ? 0x13211eu : 0x132119u);
}
