/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_cursor_097.h"
#include "ef_gui_script_095.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <stdint.h>

enum { X_POS = 0x425fc8u, Y_POS = 0x425fc6u, SELECTED = 0x161e20u };

/* 0x000f3f84, 497 bytes. Recovered callees are invoked directly with their
 * confirmed Watcom register/stack signatures; only pointer and cursor I/O
 * remains behind host adapters. */
void gui_options_display_f3f84(void)
{
    uint16_t selected = 0u;
    uint16_t hover = 0u;
    uint32_t mouse_button = 0u;
    uint32_t buttons = 0u;
    uint32_t pointer_x = 0u, pointer_y = 0u;
    uint32_t cursor_x = 0u, cursor_y = 0u;
    int32_t index;
    ef_addr32 record;
    uint32_t arg_eax, arg_edx, arg_ebx, arg_ecx, arg_stack[4];
    unsigned i;

    if (EF_U8(0x161e22u) != 0u)
        apply_request_f41c4();
    present_status_frame_f34cc();

    index = EF_S32(0x161e1cu) >> 16;
    record = 0x161d98u + (uint32_t)index * 20u;
    arg_eax = EF_U32(record);
    arg_edx = EF_U32(record + 4u);
    arg_ebx = (uint32_t)(int32_t)EF_S16(record + 8u);
    arg_ecx = (uint32_t)(int32_t)EF_S16(record + 10u);
    for (i = 0u; i != 4u; ++i)
        arg_stack[i] = (uint32_t)(int32_t)EF_S16(record + 12u + 2u * i);
    set_param_block_dd9e4(arg_eax, arg_edx, arg_ebx, arg_ecx,
                          arg_stack[0], arg_stack[1],
                          arg_stack[2], arg_stack[3]);

    for (;;) {
        if (EF_U8(SELECTED) != 0u)
            break;
        if (selected != 0u && (buttons & 1u) == 0u)
            break;

        update_remote_view_f2ac0();
        gui_display_pointer_097(&buttons, &pointer_x, &pointer_y);
        buttons |= mouse_button;

        if (selected == 0u) {
            int32_t x = (int32_t)EF_S16(X_POS);
            int32_t y = (int32_t)EF_S16(Y_POS);
            hover = (uint16_t)gui_display_hit_test_097(x, y);
        }
        if (EF_U16(0x161e18u) != 0u)
            step_animation_f36a4();
        if ((buttons & 1u) != 0u && hover != 0u && selected == 0u) {
            selected = 1u;
            /* Original DEC EAX followed by CWDE: sign-extend low 16 bits. */
            draw_record_text_f3aa8((uint32_t)(int32_t)(int16_t)(uint16_t)(hover - 1u));
            process_command_dispatch_loop();
        }
        if (EF_U8(0x4859deu) != 0u) {
            refresh_f34a0();
        } else {
            poll_mouse_ddae0();
            if (get_byte_3efa0() != 0u)
                gui_display_cursor_097(&cursor_x, &cursor_y, &mouse_button);

            if (EF_U16(X_POS) != EF_U16(0x1616eeu) ||
                EF_U16(Y_POS) != EF_U16(0x1616f0u)) {
                wait_vsync_dc2e0();
                run_shutdown_pair_dd8d0();
            }
            EF_U16(0x1616eeu) = EF_U16(X_POS);
            EF_U16(0x1616f0u) = EF_U16(Y_POS);
        }
    }

    if (EF_U8(0x4859dcu) == 0u) {
        fade_palette_out_de648(0x425c8cu);
    } else {
        call_e9a24_c3834();
        if (EF_U8(0x4859deu) == 0u)
            leave_scroll_mode_c39ac();
        call_e9a24_c386c();
    }
    if (EF_U8(0x4859deu) != 0u)
        release_pending_handle_eae10();
    toggle_page_dc250();
    toggle_page_dc250();
    restore_saved_block_dd870();
}
