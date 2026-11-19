/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_OPTIONS_096_H
#define EF_GUI_OPTIONS_096_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void gui_options_read_command_f2d74(void);
void format_global_option_line(void);

#ifdef EF_DECOMP_HOST_TEST
uint32_t gui_options_format_096(ef_addr32 format, uint32_t first,
                                uint32_t second, ef_addr32 suffix);
#endif

#ifdef __WATCOMC__
#pragma aux format_global_option_line modify [eax ebx ecx]
#pragma aux gui_options_read_command_f2d74 modify [eax]
#endif

#ifdef __cplusplus
}
#endif

#endif
