/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_cursor_097.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <stdint.h>

enum { MOTION_X = 0x420dc4, MOTION_Y = 0x420dc8,
       SCREEN_X = 0x425fc8, SCREEN_Y = 0x425fc6,
       X_REPEAT_TIMER = 0x161e24, Y_REPEAT_TIMER = 0x161e28 };

/* f3d54 uses the sign of the *16-bit* quotient to select a step. Absolute
 * thresholds are strict: 10, 20 and 35 have different boundaries. For
 * small nonzero motion, the countdown is shared with the runtime tick
 * delta, and only a negative countdown emits a step. */
static int32_t cursor_axis_step(int16_t motion, int32_t elapsed,
                                ef_addr32 timer_address)
{
    int32_t magnitude = (motion < 0) ? -(int32_t)motion : (int32_t)motion;
    if (magnitude > 35)
        return motion < 0 ? -5 : 5;
    if (magnitude > 20)
        return motion < 0 ? -2 : 2;
    if (magnitude > 10)
        return motion < 0 ? -1 : 1;
    if (motion != 0) {
        /* The original uses signed 32-bit SUB and JGE; unsigned arithmetic
         * implements wraparound without C signed-overflow undefined behavior. */
        uint32_t next = EF_U32(timer_address) - (uint32_t)elapsed;
        EF_U32(timer_address) = next;
        if ((int32_t)next < 0) {
            EF_U32(timer_address) = 30u;
            return motion < 0 ? -1 : 1;
        }
    }
    return 0;
}

/* 0x000f3d54, 560 bytes. The original accumulates x and y in two adjacent
 * words, whose overlapping dword high halves are also used for the final
 * signed 16-bit clamping. The early output additions are overwritten by
 * the final coordinates but intentionally retained for aliasing behavior. */
void gui_cursor_step_f3d54(ef_addr32 x_out, ef_addr32 y_out,
                           ef_addr32 button_out)
{
    int32_t elapsed = advance_runtime_tick(); /* original f1065c */
    int16_t x_motion, y_motion;
    int32_t x_step, y_step;

    gui_cursor_poll_input_097(); /* original indirect [0x15bab8] */
    x_motion = (int16_t)(EF_S32(MOTION_X) / 8);
    y_motion = (int16_t)(EF_S32(MOTION_Y) / 8);
    x_step = cursor_axis_step(x_motion, elapsed, X_REPEAT_TIMER);
    y_step = cursor_axis_step(y_motion, elapsed, Y_REPEAT_TIMER);

    EF_U16(x_out) = (uint16_t)(EF_U16(x_out) + (uint16_t)x_step);
    EF_U16(y_out) = (uint16_t)(EF_U16(y_out) + (uint16_t)y_step);
    EF_U16(SCREEN_X) = (uint16_t)(EF_U16(SCREEN_X) + (uint16_t)x_step);
    EF_U16(SCREEN_Y) = (uint16_t)(EF_U16(SCREEN_Y) + (uint16_t)y_step);

    /* Original reads the *overlapping dwords* at SCREEN_Y and SCREEN_Y-2,
     * shifts each right by 16 and clamps the resulting signed x/y words. */
    if (EF_S16(SCREEN_X) > 319) EF_U16(SCREEN_X) = 319u;
    if (EF_S16(SCREEN_Y) > 199) EF_U16(SCREEN_Y) = 199u;
    if (EF_S16(SCREEN_X) < 0) EF_U16(SCREEN_X) = 0u;
    if (EF_S16(SCREEN_Y) < 0) EF_U16(SCREEN_Y) = 0u;

    EF_U16(x_out) = EF_U16(SCREEN_X);
    EF_U16(y_out) = EF_U16(SCREEN_Y);
    EF_U16(button_out) = (EF_U8(0x15ba2cu) != 0u) ? 1u : 0u;
}
