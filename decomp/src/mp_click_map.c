/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x4c33c, 126 bytes. Cycle the mission-map preview page counter at
 * 0x1a53dd (0 -> 1 -> 2 -> 3 -> 0), showing the picture named by the
 * string at 0x184c14/0x184c20/0x184c2c/0x184aa4 for the new state, then
 * emit the selected message. Out-of-range values only emit the message. */
void mpClickMap(void)
{
    uint32_t name = 0u;
    uint8_t state = EF_U8(0x001a53ddu);
    if (state == 0u) {
        EF_U8(0x001a53ddu) = 1u;
        name = 0x00184c14u;
    } else if (state == 1u) {
        EF_U8(0x001a53ddu) = 2u;
        name = 0x00184c20u;
    } else if (state == 2u) {
        EF_U8(0x001a53ddu) = 3u;
        name = 0x00184c2cu;
    } else if (state == 3u) {
        EF_U8(0x001a53ddu) = 0u;
        name = 0x00184aa4u;
    }
    if (name != 0u)
        show_preview_picture_41314(name);
    ui_emit_selected_message();
}
