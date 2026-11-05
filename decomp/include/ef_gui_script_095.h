/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_SCRIPT_095_H
#define EF_GUI_SCRIPT_095_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The original comparison, decimal conversion, errno-pointer and options
 * command handlers still use host adapters until their ABIs are established.
 * The tokenizer and six-value setter below do not require a lexer adapter. */
uint32_t gui_script_guest_call_095(uint32_t target, uint32_t eax, uint32_t edx);
void gui_script_report_095(ef_addr32 format, unsigned arguments,
                           ef_addr32 token, ef_addr32 line);

uint32_t gui_script_next_token_f46cc(void);
void gui_script_rect_f5484(void);
void gui_options_dispatch_f3b78(void);

#ifdef __cplusplus
}
#endif

#endif
