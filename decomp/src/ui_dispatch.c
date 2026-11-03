/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 0x7fbec, 71 bytes. The limit is signed. Equality means no action;
 * negative indices go through the original unguarded table address math. */
void ui_dispatch_indexed_control(uint8_t code, int32_t index,
                                  int32_t value, int32_t extra)
{
    if (index < g_ui_control_limit) {
        if (g_mode_output_a != 0) {
            ef_addr32 slot = g_ui_control_table + (uint32_t)index * 4u;
            uint32_t control = read32((const uint8_t *)ef_guest_memory_ptr(slot));
            ui_control_update(code, (uint32_t)value, control, (uint32_t)extra);
        }
    } else if (index > g_ui_control_limit) {
        ui_report_out_of_range(index);
    }
}

/* 0x7fe18, 51 bytes. Initialization visits fourteen controls 0..13, once,
 * and sets the latch only when input is enabled and latch was clear. */
void ui_initialize_releases(void)
{
    unsigned i;
    if (g_mode_input_enabled_a == 0 || g_ui_control_initialized != 0) { return; }
    for (i = 0; i < 14; ++i) { ui_control_activate(i); }
    g_ui_control_initialized = 1;
}

/* 0x7fe4c, 62 bytes. Deactivate only when both gates and latch are set. */
void ui_finalize_releases(void)
{
    unsigned i;
    if (g_mode_input_enabled_a == 0 || g_mode_output_a == 0 ||
        g_ui_control_initialized == 0) { return; }
    for (i = 0; i < 14; ++i) { ui_control_deactivate(i); }
    g_ui_control_initialized = 0;
}

/* 0x7fe8c, 34 bytes. Release callbacks run regardless of initialized
 * state; only the input enable byte gates this traversal. */
void ui_release_all_controls(void)
{
    unsigned i;
    if (g_mode_input_enabled_a == 0) { return; }
    for (i = 0; i < 14; ++i) { ui_control_release(i); }
}

/* 0x802ac, 49 bytes. Compare each byte zero-extended as a WORD with CX.
 * There can be multiple matching entries, so keep scanning after a call. */
void ui_release_matching_control(uint16_t code)
{
    unsigned i;
    if (g_mode_output_a == 0) { return; }
    for (i = 0; i < 10; ++i) {
        if (g_ui_button_code_table[i] == code) { ui_control_release(i); }
    }
}

/* 0x803c4, 36 bytes. Only the original EAX/EDX/EBX/ECX tuple matters. */
void ui_reset_active_panel(void)
{
    if (g_mode_output_a != 0) { ui_control_update(0, 1, g_ui_panel_config_ptr, 0); }
}

/* 0x80400, 15 bytes. Original dispatch ends in a tail jump; keep order. */
void ui_reset_panel_and_release(void)
{
    ui_release_all_controls();
    ui_reset_panel_memory(0x001f9020);
}

/* 0x42548, 29 bytes. Two constant strings selected by a global byte. */
void ui_emit_selected_message(void)
{
    ui_show_message(g_ui_alternate_message ? 0x001843f4u : 0x001843f0u);
}
