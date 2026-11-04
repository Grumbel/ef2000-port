/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Widget list refresh and record counting. */

/* 0x69b70, 106 bytes. Walk the widget chain starting at the named widget at 0x186320 under the object at 0x186314; for each entry whose weapon flags test true, show it and set its name from the 0x91-byte table at 0x12bc68+0x122. */
void refresh_weapon_widgets_69b70(void)
{
    int16_t index = 2;
    uint32_t offset = 0x122u;
    ef_addr32 widget = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00186314u), 0x00186320u);

    if (widget == 0)
        return;
    while (EF_U32(offset + 0x0012bc68u) != 0) {
        if (weapon_flags_any_69a40(index) != 0) {
            guest_fn_a7588_v(widget, EF_U32(offset + 0x0012bc68u));
            set_object_name_a75cc(widget, EF_U32(offset + 0x0012bc68u));
            widget = EF_U32(widget + 0x21u);
        }
        offset += 0x91u;
        index++;
        if (widget == 0)
            break;
    }
}

/* 0xfaafc, 130 bytes. Count the active elite 0x15-byte entries of a group whose code is not in the set; the original then always returns 1. */
uint32_t count_group_entries_faafc(ef_addr32 group)
{
    int16_t count = 0;
    int16_t i;

    for (i = 0; i < EF_S16(group + 0x2au); i++) {
        ef_addr32 entry = EF_U32(group + 0x2cu) + (uint32_t)((int32_t)i * 0x15);
        if (is_active_bec84(entry) != 0 && (uint16_t)record_is_elite(entry) != 0
            && (uint16_t)code_in_set_bcb50(EF_U8(entry)) == 0)
            count++;
    }
    /* ORIGINAL BUG: the count is clamped up to 3 and then any value above 1 yields 1, so the result is always 1. */
    (void)count;
    return 1;
}
