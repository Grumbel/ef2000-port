/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x340a4, 153 bytes. EAX=root, all other GPRs preserved.
 * Count is signed and reread for each row; next links are read after show. */
void show_network_service_list_340a4(ef_addr32 root)
{
    ef_addr32 widget = find_widget_checked_a78bc(root, 0x00182d94u);
    ef_addr32 text = 0x00428cf8u;
    uint32_t index = 0;
    int32_t count;
    uint32_t height;

    for (;;) {
        count = (int32_t)EF_U32(0x00428cf4u);
        if ((int32_t)index >= count)
            break;
        show_widget_a7588(widget, text);
        ++index;
        text += 0x41u;
        widget = EF_U32(widget + offsetof(ef_widget_link, next));
    }
    EF_U16(root + offsetof(ef_widget_link, content_height)) =
        (uint16_t)(EF_U16(0x00428cf4u) * 20u);
    if (count < 5) {
        height = (uint32_t)(int32_t)(int16_t)(EF_U16(0x00428cf4u) * 20u + 24u);
        resize_widget_a520c(root, 0x11cu, height, 0u);
        toggle_children_a570c(root, 0u);
    } else {
        resize_widget_a520c(root, 0x12cu, 0x7cu, 0u);
        toggle_children_a570c(root, 1u);
    }
}
