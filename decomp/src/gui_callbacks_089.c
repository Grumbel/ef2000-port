/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Every guest call records its original address. The host-only hook is a
 * semantic boundary; EDX preservation across an unresolved call is not yet
 * a confirmed guest calling convention. */
static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx)
{
    return gui_guest_call_089(entry, eax, edx);
}

/* 0x4b54, 39 bytes, two alternative external JMPs at 487c / ac7a8. */
void gui_kots_init_4b54(ef_addr32 widget)
{
    uint32_t child = EF_U32(widget + 0x51u);
    uint32_t decision;
    guest(0xa4b80u, EF_U8(child + 0x10u), 0u);
    decision = guest(0x432cu, 0u, 0u);
    if ((uint8_t)decision != 0u)
        guest(0x487cu, decision, 0u);
    else
        guest(0xac7a8u, 0x17f79cu, 0u);
}

/* 0x4b1a4, 75 bytes: reject when network is active or signed fuel below
 * minimum, otherwise close the dialog and jump to the next screen. */
void gui_campaign_warning_ok_4b1a4(void)
{
    uint32_t fuel;
    if (EF_U8(0x126576u) != 0u) {
        guest(0x4f388u, 0x132202u, 0x132223u);
        return;
    }
    fuel = guest(0x50770u, 0u, 0u);
    if ((int16_t)fuel < EF_S16(0x1357b0u)) {
        guest(0x4f388u, 0x1321b7u, 0x1321d8u);
    } else {
        guest(0xa4c84u, 0u, 0u);
        guest(0xac7a8u, 0x1849bcu, 0u);
    }
}

/* 0x50638, 15 bytes: one call followed by a different terminal jump. */
void gui_draw_fuel_bar_50638(void)
{
    guest(0xac9e8u, 0u, 0u);
    guest(0xa4db0u, 0x184ec4u, 0u);
}

/* 0x5089c, 0x508fc and 0x5095c: three distinct 96-byte callback bodies.
 * The widget's two ASCII digits encode a 1-based selection. This is not
 * atoi: second digit is read only if first digit is exactly '1'. */
static void type_click(ef_addr32 widget, uint32_t group)
{
    uint32_t item = (uint32_t)EF_U8(widget + 0x42u) - 0x30u;
    uint32_t result;
    if ((int16_t)item == 1) {
        uint32_t digit = EF_U8(widget + 0x43u);
        if (digit >= 0x30u && digit <= 0x39u)
            item = digit - 0x26u;
    }
    result = guest(0x50a1cu, group, item);
    if ((uint8_t)result != 0u) {
        guest(0x4f314u, 0x1326e7u, 0x132704u);
        return;
    }
    result = guest(0x50b68u, group, item & 0xffu);
    guest(0x5086cu, result, item);
}

void gui_type2_click_5089c(ef_addr32 widget)
{
    type_click(widget, 3u);
}

void gui_type3_click_508fc(ef_addr32 widget)
{
    type_click(widget, 4u);
}

void gui_type4_click_5095c(ef_addr32 widget)
{
    type_click(widget, 1u);
}

/* 0x84ddc, 13 bytes: widget child ID followed by external terminal JMP. */
void gui_show_summary_info_84ddc(ef_addr32 widget)
{
    guest(0xa4b80u, EF_U8(widget + 0x10u), 0u);
}

/* 0xda2b0, 8 bytes: enable cloud rendering. */
void env_clouds_da2b0(void)
{
    EF_U8(0x425c88u) = 1u;
}
