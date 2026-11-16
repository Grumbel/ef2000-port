/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb574, 89 bytes. EAX widget, EAX scratch; six GPRs preserved, RET. */
void gui_close_target_view_b574(ef_addr32 widget)
{
    ef_addr32 text = widget + 0x3fu;
    ef_addr32 target;

    /* ORIGINAL BUG: both calls compare the exact same two strings.
     * Keep the second call and the nominal mode-one branch. */
    if (guest_strcmp(text, 0x00180834u) != 0)
        EF_U8(0x00121e95u) = 2u;
    else if (guest_strcmp(text, 0x00180834u) != 0)
        EF_U8(0x00121e95u) = 1u;
    else
        EF_U8(0x00121e95u) = 0u;
    target = find_named_or_abort_a46cc(0x001805f0u);
    redraw_item_a5ef8(EF_U8(target + 0x10u));
}
