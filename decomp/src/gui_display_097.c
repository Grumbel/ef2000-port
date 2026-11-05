/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_cursor_097.h"
#include "ef_mem.h"

#include <stdint.h>

enum { X_POS = 0x425fc8u, Y_POS = 0x425fc6u, SELECTED = 0x161e20u };

static uint32_t guest(uint32_t target, uint32_t eax)
{
    return gui_display_guest_call_097(target, eax);
}

/* 0x000f3f84, 497 bytes. The display loop processes a pointer button and
 * a second (polling) button source. A selected action is allowed to keep
 * running only while the first bit remains pressed. All hardware drawing,
 * input and command actions are kept as explicit host adapters. */
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
    struct gui_display_params_097 args;
    unsigned i;

    if (EF_U8(0x161e22u) != 0u)
        guest(0xf41c4u, 0u);
    guest(0xf34ccu, 0u);

    index = EF_S32(0x161e1cu) >> 16;
    record = 0x161d98u + (uint32_t)index * 20u;
    args.eax = EF_U32(record);
    args.edx = EF_U32(record + 4u);
    args.ebx = (uint32_t)(int32_t)EF_S16(record + 8u);
    args.ecx = (uint32_t)(int32_t)EF_S16(record + 10u);
    for (i = 0u; i != 4u; ++i)
        args.stack[i] = (uint32_t)(int32_t)EF_S16(record + 12u + 2u * i);
    gui_display_set_params_097(&args); /* original dd9e4 */

    for (;;) {
        if (EF_U8(SELECTED) != 0u)
            break;
        if (selected != 0u && (buttons & 1u) == 0u)
            break;

        guest(0xf2ac0u, 0u);
        gui_display_pointer_097(&buttons, &pointer_x, &pointer_y);
        buttons |= mouse_button;

        if (selected == 0u) {
            int32_t x = (int32_t)EF_S16(X_POS);
            int32_t y = (int32_t)EF_S16(Y_POS);
            hover = (uint16_t)gui_display_hit_test_097(x, y);
        }
        if (EF_U16(0x161e18u) != 0u)
            guest(0xf36a4u, 0u);
        if ((buttons & 1u) != 0u && hover != 0u && selected == 0u) {
            selected = 1u;
            /* Original DEC EAX followed by CWDE: sign-extend low 16 bits. */
            guest(0xf3aa8u, (uint32_t)(int32_t)(int16_t)(uint16_t)(hover - 1u));
            guest(0xf3b78u, 0u);
        }
        if (EF_U8(0x4859deu) != 0u) {
            guest(0xf34a0u, 0u);
        } else {
            guest(0xddae0u, 0u);
            if ((guest(0x3efa0u, 0u) & 0xffu) != 0u)
                gui_display_cursor_097(&cursor_x, &cursor_y, &mouse_button);

            if (EF_U16(X_POS) != EF_U16(0x1616eeu) ||
                EF_U16(Y_POS) != EF_U16(0x1616f0u)) {
                guest(0xdc2e0u, 0u);
                guest(0xdd8d0u, 0u);
            }
            EF_U16(0x1616eeu) = EF_U16(X_POS);
            EF_U16(0x1616f0u) = EF_U16(Y_POS);
        }
    }

    if (EF_U8(0x4859dcu) == 0u) {
        guest(0xde648u, 0x425c8cu);
    } else {
        guest(0xc3834u, 0u);
        if (EF_U8(0x4859deu) == 0u)
            guest(0xc39acu, 0u);
        guest(0xc386cu, 0u);
    }
    if (EF_U8(0x4859deu) != 0u)
        guest(0xeae10u, 0u);
    guest(0xdc250u, 0u);
    guest(0xdc250u, 0u);
    guest(0xdd870u, 0u);
}
