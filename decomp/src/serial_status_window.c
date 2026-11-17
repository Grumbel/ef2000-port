/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x33b5c, 244 bytes. EAX=root; refresh pending serial status fields.
 * The reply pointer is reloaded after showing it, then stored unconditionally. */
void refresh_serial_status_33b5c(ef_addr32 root)
{
    char stage[64];
    ef_addr32 widget;
    ef_addr32 text;
    uint32_t offset;
    uint8_t character;

    if (EF_U8(0x0015f686u) != EF_U8(0x0012598cu)) {
        EF_U8(0x0012598cu) = EF_U8(0x0015f686u);
        format_ui_status_value((ef_addr32)(uintptr_t)stage, 0x00182bc0u,
                              EF_U32(0x00125968u + EF_U8(0x0015f686u) * 4u));
        widget = find_widget_checked_a78bc(root, 0x00182c94u);
        show_widget_a7588(widget, (ef_addr32)(uintptr_t)stage);
    }
    if (EF_U8(0x0015f694u) != 0) {
        text = EF_U32(0x0015f690u);
        if (text != 0) {
            EF_U8(0x0015f694u) = 0;
            widget = find_widget_checked_a78bc(root, 0x00182c9cu);
            show_widget_a7588(widget, text);
        }
    }
    text = EF_U32(0x0015f68cu);
    if (EF_U8(0x0015f695u) != 0 && text != 0) {
        EF_U8(0x0015f695u) = 0;
        for (offset = 0; offset < 31u; ++offset) {
            character = EF_U8(text + offset);
            if (character == 13u || character == 255u)
                break;
        }
        /* ORIGINAL BUG: NUL does not terminate the reply scan. */
        EF_U8(text + offset) = 0;
        EF_U32(0x0015f68cu) = text;
        widget = find_widget_checked_a78bc(root, 0x00182ca8u);
        show_widget_a7588(widget, text);
        text = EF_U32(0x0015f68cu);
    }
    EF_U32(0x0015f68cu) = text;
}
