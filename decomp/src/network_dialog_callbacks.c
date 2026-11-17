/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x345d4, 19 bytes. The disabled branch shares the preceding RET. */
void net_new_end_string_345d4(void)
{
    if (EF_U8(0x13578cu) != 0u)
        apply_a4cb4_a4d90(0x182e10u);
}

/* 0x347ac, 13 bytes. The widget ID is zero-extended before the tail call. */
void net_open_loops_347ac(ef_addr32 widget)
{
    close_widget_a4b80(EF_U8(widget + 0x10u));
}
