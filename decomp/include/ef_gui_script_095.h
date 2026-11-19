/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_SCRIPT_095_H
#define EF_GUI_SCRIPT_095_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The parser token comparator and OPTIONS command handlers use their
 * recovered direct C entry points. */
void gui_script_report_095(ef_addr32 format, unsigned arguments,
                           ef_addr32 token, ef_addr32 line);

uint32_t gui_script_next_token_f46cc(void);
void gui_script_rect_f5484(void);
void process_command_dispatch_loop(void);

#ifdef __cplusplus
}
#endif

#endif
