/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
/* Original 5086c has no inputs, saves EBX/ECX/EDX and leaves ESI/EDI/EBP. */
#pragma aux guest_fn_5086c_w parm [] modify [eax]
#endif

/* Shared logic of three 96-byte callbacks. EAX widget input, EAX scratch;
 * six other registers preserved and plain RET at each original entry. */
static void select_widget_type(ef_addr32 widget, uint32_t group)
{
    uint32_t item = (uint32_t)EF_U8(widget + 66u) - 48u;
    uint32_t digit;

    if ((int16_t)item == 1) {
        digit = EF_U8(widget + 67u);
        if (digit >= 48u && digit <= 57u)
            item = digit - 38u;
    }
    if (tactical_lookup_contains((uint8_t)group) != 0u) {
        update_dialog_bindings(0x001326e7u, 0x00132704u);
        return;
    }
    set_slot_state_50b68(group, item & 255u);
    guest_fn_5086c_w();
}

/* Registry mpType2Click, group three. */
void gui_type2_click_5089c(ef_addr32 widget)
{
    select_widget_type(widget, 3u);
}

/* Registry mpType3Click, group four. */
void gui_type3_click_508fc(ef_addr32 widget)
{
    select_widget_type(widget, 4u);
}

/* Registry mpType4Click, group one. */
void gui_type4_click_5095c(ef_addr32 widget)
{
    select_widget_type(widget, 1u);
}
