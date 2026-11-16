/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x4cf4, 86 bytes. EAX widget, EAX scratch; six GPRs preserved, RET. */
void gui_change_difficulty_4cf4(ef_addr32 widget)
{
    ef_addr32 target;
    uint32_t row;
    int32_t selection;

    EF_U16(0x00121e46u) = (uint16_t)parse_int(widget + 0x3fu);
    target = find_named_or_abort_a46cc(0x0017f7a4u);
    /* Read the overlapping signed high word after the lookup call. */
    selection = (int32_t)EF_U32(0x00121e44u) >> 16;
    row = ((uint32_t)selection - 1u) * 76u;
    row += EF_U32(0x00121e40u);
    set_session_text_7c4fc(row + 0x28u, target);
    /* The session call may change the widget's ID. */
    redraw_item_a5ef8(EF_U8(target + 0x10u));
}

/* 0xa974, 49 bytes. EAX widget, EAX scratch; six GPRs preserved, RET. */
void gui_zoom_map_a974(ef_addr32 widget)
{
    uint16_t mode = (uint16_t)(1u - EF_U16(0x00121e8eu));
    EF_U16(0x00121e8eu) = mode;
    show_widget_a7588(widget, mode != 0u ? 0x00132119u : 0x0013211eu);
}
