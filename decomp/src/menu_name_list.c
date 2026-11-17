/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x347bc, 136 bytes. EAX=root; saved EBX/ECX/EDX/ESI, callees preserve
 * EDI/EBP. Format the first identifier, then at most four sibling values.
 * Read each next link after the text/show call, which may update the list. */
void refresh_menu_name_list_347bc(ef_addr32 root)
{
    char text[16];
    ef_addr32 buffer = (ef_addr32)(uintptr_t)text;
    ef_addr32 widget = find_widget_checked_a78bc(root, 0x00182e58u);
    uint32_t offset;
    uint32_t value;

    (void)guest_sprintf_a4342(buffer, 0x00182e38u, EF_U32(0x001357c0u));
    show_widget_a7588(widget, buffer);
    widget = EF_U32(widget + offsetof(ef_widget_link, next));
    for (offset = 0u; offset < 16u; offset += 4u) {
        if (widget == 0u)
            return;
        value = EF_U32(0x0013577cu + offset);
        if (value != 0u)
            (void)guest_sprintf_a4342(buffer, 0x00182e38u, value);
        else
            text[0] = 0;
        show_widget_a7588(widget, buffer);
        widget = EF_U32(widget + offsetof(ef_widget_link, next));
    }
}
