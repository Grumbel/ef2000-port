/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_CURSOR_097_H
#define EF_GUI_CURSOR_097_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host adapter for the unknown no-argument guest call through [0x15bab8]. */
void gui_cursor_poll_input_097(void);

/* Original register arguments EAX, EDX, EBX point to three output words. */
void gui_cursor_step_f3d54(ef_addr32 x_out, ef_addr32 y_out,
                           ef_addr32 button_out);

/* f3f84 forwards a packed 20-byte record to dd9e4. */
struct gui_display_params_097 {
    uint32_t eax, edx, ebx, ecx, stack[4];
};

void gui_display_set_params_097(const struct gui_display_params_097 *args);
uint32_t gui_display_guest_call_097(uint32_t target, uint32_t eax);
void gui_display_pointer_097(uint32_t *buttons, uint32_t *x, uint32_t *y);
uint32_t gui_display_hit_test_097(int32_t x, int32_t y);
void gui_display_cursor_097(uint32_t *x, uint32_t *y, uint32_t *button);
void gui_options_display_f3f84(void);

#ifdef __cplusplus
}
#endif

#endif
