/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Keyboard/joystick-style GUI cursor stepping on the 320x200 menu
 * screen. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define CURSOR_REPEAT_TICKS 30

/* The cursor step for one axis: the motion (already divided by 8) moves
 * 5, 2 or 1 pixels above 35, 20 and 10. Smaller nonzero motion counts
 * the repeat timer down by the elapsed ticks and steps one pixel each
 * time it drops below zero. */
static int16_t cursor_axis_step(int16_t motion, int32_t elapsed, int32_t *repeat)
{
    int16_t direction = motion < 0 ? -1 : 1;

    if ((int32_t)abs_a28f2(motion) > 35)
        return (int16_t)(direction * 5);
    if ((int32_t)abs_a28f2(motion) > 20)
        return (int16_t)(direction * 2);
    if ((int32_t)abs_a28f2(motion) > 10)
        return direction;
    if (motion == 0)
        return 0;
    *repeat = (int32_t)((uint32_t)*repeat - (uint32_t)elapsed);
    if (*repeat >= 0)
        return 0;
    *repeat = CURSOR_REPEAT_TICKS;
    return direction;
}

/* 0xf3d54, 560 bytes. Poll the input, step the GUI cursor by the motion
 * of both axes, clamp it to the screen and return the position and the
 * button state through the three word pointers. The step is first added
 * to *X_OUT and *Y_OUT as well; the clamped position then overwrites
 * them. */
void gui_cursor_step_f3d54(ef_addr32 x_out, ef_addr32 y_out, ef_addr32 button_out)
{
    int32_t elapsed = (int16_t)advance_runtime_tick();
    int16_t x_step;
    int16_t y_step;
    uint16_t y;
    uint16_t cursor_x;

    guest_callptr0_x(g_input_poll_hook);
    x_step = cursor_axis_step((int16_t)(g_mouse_motion_x / 8), elapsed, &g_cursor_repeat_x);
    y_step = cursor_axis_step((int16_t)(g_mouse_motion_y / 8), elapsed, &g_cursor_repeat_y);

    EF_U16(x_out) = (uint16_t)(EF_U16(x_out) + (uint16_t)x_step);
    y = (uint16_t)(EF_U16(y_out) + (uint16_t)y_step);
    cursor_x = (uint16_t)g_menu_cursor_x;
    EF_U16(y_out) = y;
    g_menu_cursor_x = (int16_t)(cursor_x + (uint16_t)x_step);
    g_menu_cursor_y = (int16_t)((uint16_t)g_menu_cursor_y + (uint16_t)y_step);

    if (g_menu_cursor_x > 319)
        g_menu_cursor_x = 319;
    if (g_menu_cursor_y > 199)
        g_menu_cursor_y = 199;
    if (g_menu_cursor_x < 0)
        g_menu_cursor_x = 0;
    if (g_menu_cursor_y < 0)
        g_menu_cursor_y = 0;

    EF_U16(x_out) = (uint16_t)g_menu_cursor_x;
    EF_U16(y_out) = (uint16_t)g_menu_cursor_y;
    EF_U16(button_out) = g_mouse_button_down != 0u;
}
