/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x33888, 228 bytes. EAX=root; preserves all GPRs except scratch EAX.
 * Keep formatting before lookup and reread FIFO mode after the final lookup. */
void show_modem_settings_33888(ef_addr32 root)
{
    char text[16];
    ef_addr32 buffer = (ef_addr32)(uintptr_t)text;
    ef_addr32 widget;
    int32_t rate;

    (void)guest_sprintf_a4342(buffer, 0x00182bbcu,
                            (uint32_t)EF_U8(0x0015f07fu) + 1u);
    widget = find_widget_checked_a78bc(root, 0x00182bf0u);
    show_widget_a7588(widget, buffer);
    rate = rate_divisor_df2b0();
    (void)guest_sprintf_a4342(buffer, 0x00182bfcu, rate);
    widget = find_widget_checked_a78bc(root, 0x00182c00u);
    show_widget_a7588(widget, buffer);
    (void)guest_sprintf_a4342(buffer, 0x00182bc0u,
                            EF_U32(0x00125960u + (uint32_t)EF_U8(0x0015f099u) * 4u));
    widget = find_widget_checked_a78bc(root, 0x00182c0cu);
    show_widget_a7588(widget, buffer);
    widget = find_widget_checked_a78bc(root, 0x00182c18u);
    if (EF_U8(0x0015f099u) != 0) {
        (void)guest_sprintf_a4342(buffer, 0x00182bbcu,
                                EF_U32(0x000335b0u + (uint32_t)EF_U8(0x0015f09au) * 4u));
        show_widget_a7588(widget, buffer);
    } else {
        show_widget_a7588(widget, 0x00182c24u);
    }
}
