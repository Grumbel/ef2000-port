/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x4b1a4,75 bytes. No inputs; EAX scratch, six preserved registers,
 * plain RET. Original registry cgWarningOkClick. */
void gui_campaign_warning_ok_4b1a4(void)
{
    int16_t difference;

    if (EF_U8(0x00126576u) != 0u) {
        bind_dialog_values_4f388(0x00132202u, 0x00132223u);
        return;
    }
    difference = (int16_t)max_difference_50770();
    if (difference < EF_S16(0x001357b0u)) {
        bind_dialog_values_4f388(0x001321b7u, 0x001321d8u);
        return;
    }
    refresh_all_widgets_a4c84();
    set_pending_name_ac7a8(0x001849bcu);
}

/* 0x50638,15 bytes. No inputs; six preserved registers, EAX widget
 * pointer result from terminal a4db0, plain RET. Registry mpDrawFuelBar. */
ef_addr32 gui_draw_fuel_bar_50638(void)
{
    reset_view_ac9e8();
    return apply_a4b80_a4db0(0x00184ec4u);
}
