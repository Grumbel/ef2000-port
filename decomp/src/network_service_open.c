/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/network_selection.h"

/* 0x34140, 151 bytes. EAX=selection record; EBX/EDX explicitly saved.
 * Other callee clobbers are scratch. No bounds check on the selected index. */
void open_network_service_34140(ef_addr32 selection)
{
    ef_addr32 name;
    ef_addr32 window;
    ef_addr32 widget;

    EF_U32(0x00428cf0u) = guest_fn_9d4cd(selection + offsetof(ef_network_selection, number)) - 1u;
    if (guest_fn_e1780() == 0) {
        if (guest_fn_e1744(EF_U32(0x00428cf0u)) == 0)
            ui_report_missing_record(0x00182d98u);
    }
    name = 0x00428cf8u + EF_U32(0x00428cf0u) * 65u;
    window = find_named_or_abort_a46cc(0x00182cf4u);
    widget = find_widget_checked_a78bc(window, 0x00182c40u);
    show_widget_a7588(widget, name);
    EF_U8(0x00125936u) = 1;
    name = 0x00428cf8u + EF_U32(0x00428cf0u) * 65u;
    (void)copy_string_9d749(0x00125937u, name);
    guest_fn_a4db0(0x00182db8u);
}
