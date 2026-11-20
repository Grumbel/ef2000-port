/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* Copy the widget's text after its leading class-2 characters into
 * g_network_session_title.
 * ORIGINAL BUG: after 32768 skipped bytes the signed word index wraps
 * behind the string. Reload the widget text pointer on every iteration. */
static ef_addr32 copy_session_title(const ef_widget_link *control)
{
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

/* 0x3444c, 49 bytes plus the tail it shares with 0x34218. Callback slot
 * 0x12bbb0; EAX=widget, EAX=resulting title address. */
ef_addr32 accept_network_session_title_3444c(ef_addr32 widget)
{
    return copy_session_title((const ef_widget_link *)(uintptr_t)widget);
}

/* 0x34218, 125 bytes, GUI callback "opengamesell". The widget's number
 * label selects a DirectPlay game (one-based): open the "gamesell" dialog,
 * show the game's row in "game_cli" of "DPSettings" and take the widget's
 * text as the session title. */
void opengamesell_34218(ef_addr32 widget)
{
    const ef_widget_link *control = (const ef_widget_link *)(uintptr_t)widget;
    ef_addr32 list;

    g_directplay_selected_game = parse_int((ef_addr32)(uintptr_t)&control->number_label) - 1;
    apply_a4b80_a4db0(0x00182dc4u);                          /* "gamesell" */
    list = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00182cf4u),  /* "DPSettings" */
                                     0x00182c34u);           /* "game_cli" */
    show_widget_a7588(list, (ef_addr32)(uintptr_t)g_directplay_rows[g_directplay_selected_game]);
    copy_session_title(control);
}
