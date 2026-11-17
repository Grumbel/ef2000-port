/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x341d8, 62 bytes. EAX=root. The signed count is reloaded each iteration,
 * and the next widget link is read after the display callback returns. */
void net_directplay_game_selected_341d8(ef_addr32 root)
{
    ef_addr32 child = find_widget_checked_a78bc(root, 0x00182d94u);
    ef_addr32 row = 0x004288f0u;
    uint32_t index = 0;
    while ((int32_t)index < EF_S32(0x00429210u)) {
        show_widget_a7588(child, row);
        ++index;
        row += 64u;
        child = EF_U32(child + 0x21u);
    }
}
