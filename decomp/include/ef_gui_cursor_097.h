/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_CURSOR_097_H
#define EF_GUI_CURSOR_097_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Testable UI input and cursor adapters used by the recovered display loop. */
void gui_display_pointer_097(uint32_t *buttons, uint32_t *x, uint32_t *y);
uint32_t gui_display_hit_test_097(int32_t x, int32_t y);
void gui_display_cursor_097(uint32_t *x, uint32_t *y, uint32_t *button);
void gui_options_display_f3f84(void);

#ifdef __cplusplus
}
#endif

#endif
