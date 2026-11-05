/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_SCRIPT_094_H
#define EF_GUI_SCRIPT_094_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original guest callees remain host adapters, not confirmed OpenWatcom
 * signatures: token reader f46cc, decimal parser 9d4cd, error pointer ebd30,
 * path utilities 9c980/9ca57, and driver selection 0xc3974/80 etc. */
uint32_t gui_script_guest_call_094(uint32_t target, uint32_t eax, uint32_t edx);
void gui_script_report_094(ef_addr32 message, unsigned arguments,
                           ef_addr32 token, ef_addr32 line);
ef_addr32 gui_script_path_buffer_094(void);

void gui_script_size_f4db8(void);
void gui_script_from_f4ed4(void);
void gui_script_backdrop_f4ff0(void);
void gui_script_action_f50ec(void);
void gui_script_pointer_f5180(void);
void gui_script_gadgetry_f5214(void);
void gui_script_name_f53bc(void);

#ifdef __cplusplus
}
#endif

#endif
