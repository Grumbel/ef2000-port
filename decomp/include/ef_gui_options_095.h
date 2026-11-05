/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUI_OPTIONS_095_H
#define EF_GUI_OPTIONS_095_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host stand-ins for fc795, e8651, and fa9d2; the source itself reads and
 * writes the original guest option table. No guest allocator ABI is claimed. */
ef_addr32 gui_options_allocate_095(uint32_t helper, ef_addr32 old, uint32_t size);
void gui_options_release_095(ef_addr32 address);

int32_t gui_options_find_f44b6(ef_addr32 assignment, int32_t remove);
int32_t gui_options_store_f4347(ef_addr32 assignment);

#ifdef __cplusplus
}
#endif

#endif
