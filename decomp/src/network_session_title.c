/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* Shared epilogues are expressed as ordinary C calls followed by return;
 * their caller's saved registers belong to the entry, not to the tail.
 * 0x3444c, 49 bytes plus shared tail 0x34287..0x34294.
 * Callback slot 0x12bbb0; EAX=widget, EAX=resulting title address.
 * ORIGINAL BUG: after 32768 skipped bytes the signed word index wraps
 * behind the string. Reload the widget text pointer on every iteration. */
ef_addr32 accept_network_session_title_3444c(ef_addr32 widget)
{
    const ef_widget_link *control = (const ef_widget_link *)(uintptr_t)widget;
    uint32_t index = 0;
    ef_addr32 source;

    for (;;) {
        source = control->text + (uint32_t)(int32_t)(int16_t)(uint16_t)index;
        if ((g_character_class_table[(uint8_t)(EF_U8(source) + 1u)] & 2u) == 0)
            break;
        ++index;
    }
    return copy_string_9d749((ef_addr32)(uintptr_t)g_network_session_title, source);
}
