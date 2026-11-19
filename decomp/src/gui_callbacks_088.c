/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xc838, 48 bytes. Advance the signed group selection, wrap it at
 * the group count, and redraw the debrief page. */
void gui_show_debrief_c838(void)
{
    uint16_t next = (uint16_t)(g_debrief_group + 1u);
    int16_t count = g_match_group_count;

    g_debrief_group = next;
    if ((int16_t)next >= count)
        g_debrief_group = 0u;
    rebuild_page_c868();
}

/* 0x4318 (17 bytes): EDX carries the incoming widget across the first call. */
void gui_mainloop_4318(ef_addr32 widget)
{
    hiscore_load_41fc();
    hiscore_refresh_display_40dc(widget);
}

/* 0xa960 (19 bytes): both alternatives tail-jump out of this function. */
void gui_change_mode_a960(ef_addr32 widget)
{
    if (EF_U16(0x121e8eu) != 0u)
        open_hiscores_a870(widget);
    else
        generate_gui_help_4595c(widget);
}

/* 0xbde8 (93 bytes), four ID lookups and selector calls. Last selector is
 * a terminal jump in guest code, not an additional widget lookup. */
void gui_click_pilot1_bde8(void)
{
    static const ef_addr32 names[] = { 0x1808f4u, 0x1808fcu,
                                        0x180904u, 0x18090cu };
    unsigned i;
    for (i = 0; i < 4u; ++i) {
        ef_addr32 child = find_named_or_abort_a46cc(names[i]);
        close_widget_a4b80(EF_U8(child + 0x10u));
    }
}

/* Pilot 2/3/4 callbacks are three distinct named functions. The only
 * difference in their selected branch is the 67-byte row offset, slot,
 * widget and per-row fallback. */
static void click_pilot(ef_addr32 widget, uint32_t row,
                        uint8_t slot, ef_addr32 selected, ef_addr32 other)
{
    if (guest_strcmp(widget + 0x3fu, 0x180914u) == 0) {
        randomize_pilot_779f4(EF_U32(0x197668u) + row);
        fill_record_dialog_bc38(selected, slot);
    } else {
        ef_addr32 child = find_named_or_abort_a46cc(other);
        close_widget_a4b80(EF_U8(child + 0x10u));
    }
}

/* 0xbe48, 67 bytes: randomize and fill the first pilot slot. */
void gui_click_pilot2_be48(ef_addr32 widget)
{
    click_pilot(widget, 0u, 0u, 0x1808f4u, 0x180918u);
}

/* 0xbe8c, 73 bytes: second row, fill slot 1. */
void gui_click_pilot3_be8c(ef_addr32 widget)
{
    click_pilot(widget, 0x43u, 1u, 0x1808fcu, 0x180924u);
}

/* 0xbed8, 75 bytes: third row, fill slot 2. */
void gui_click_pilot4_bed8(ef_addr32 widget)
{
    click_pilot(widget, 0x86u, 2u, 0x180904u, 0x18092cu);
}

/* 0xbf24, 75 bytes: fourth row, fill slot 3. */
void gui_save_conflict_bf24(ef_addr32 widget)
{
    click_pilot(widget, 0xc9u, 3u, 0x18090cu, 0x180934u);
}

/* 0xca40 (25 bytes): width/height pair copied in reverse address order. */
uint32_t gui_alter_size_ca40(uint32_t eax)
{
    union {
        uint32_t eax;
        uint16_t word[2];
    } result;
    result.eax = eax;
    EF_U16(0x1a4ba2u) = EF_U16(0x1a365eu);
    result.word[0] = EF_U16(0x1a3660u);
    EF_U16(0x1a4ba0u) = result.word[0];
    return result.eax;
}
