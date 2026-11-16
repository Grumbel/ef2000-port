/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x5086c: entry 5086c..5089a, continuation 50a52..50b34 (273 bytes).
 * No inputs; EAX scratch, six other GPRs preserved, plain RET.
 * The intervening bytes belong to four callbacks and their lookup helper. */
void refresh_type_widgets_5086c(void)
{
    char name[20];
    ef_addr32 root = find_named_or_abort_a46cc(0x00184af0u);
    uint32_t slot;
    ef_addr32 format;

    clear_children_dirty(root);
    for (slot = 1u; slot <= 4u; ++slot) {
        uint32_t kind = EF_U8(0x001a553bu + slot);
        switch (kind) {
        case 0u: format = 0x00184ed8u; break;
        case 1u: format = 0x00184ee0u; break;
        case 2u: format = 0x00184ee8u; break;
        case 3u: format = 0x00184ef0u; break;
        case 4u: format = 0x00184ef8u; break;
        case 5u: format = 0x00184f00u; break;
        case 18u: format = 0x00184f08u; break;
        case 7u: format = 0x00184f10u; break;
        case 6u: format = 0x00184f18u; break;
        case 16u: format = 0x00184f20u; break;
        case 17u: format = 0x00184f28u; break;
        case 22u: format = 0x00184f30u; break;
        default: format = 0u; break;
        }
        /* ORIGINAL BUG: an unsupported kind reuses the previous name;
         * if the first kind is unsupported the stack buffer is uninitialized. */
        if (format != 0u)
            guest_sprintf_a4342((ef_addr32)(uintptr_t)name, format, slot);
        enable_widget_a72d8(find_widget_checked_a78bc(
            root, (ef_addr32)(uintptr_t)name));
    }
}
