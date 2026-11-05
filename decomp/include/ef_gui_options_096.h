/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_OPTIONS_096_H
#define EF_GUI_OPTIONS_096_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original indirect formatter at [0x132a74]; this remains a host adapter. */
uint32_t gui_options_format_096(ef_addr32 format, uint32_t first,
                                uint32_t second, ef_addr32 suffix);

void gui_options_read_command_f2d74(void);
uint32_t gui_options_format_global_f6124(void);

#ifdef __cplusplus
}
#endif

#endif
