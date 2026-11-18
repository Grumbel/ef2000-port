/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

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

#ifdef EF_DECOMP_HOST_TEST

#include "ef_mem.h"
/* Direct original addresses are kept at each unresolved guest-call boundary.
 * Only locally written registers are authoritative. This host-only hook is
 * not the x86 ABI and guest differential verification remains pending. */
static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    return gui_guest_call_088(entry, eax, edx, ebx);
}

/* 0x4318 (17 bytes): EDX carries the incoming widget across the first call. */
void gui_mainloop_4318(ef_addr32 widget)
{
    guest(0x41fcu, widget, widget, 0u);
    guest(0x40dcu, widget, widget, 0u);
}

/* 0xa960 (19 bytes): both alternatives tail-jump out of this function. */
void gui_change_mode_a960(ef_addr32 widget)
{
    guest(EF_U16(0x121e8eu) ? 0xa870u : 0x4595cu, widget, 0u, 0u);
}

/* 0xbde8 (93 bytes), four ID lookups and selector calls. Last selector is
 * a terminal jump in guest code, not an additional widget lookup. */
void gui_click_pilot1_bde8(void)
{
    static const ef_addr32 names[] = { 0x1808f4u, 0x1808fcu,
                                        0x180904u, 0x18090cu };
    unsigned i;
    for (i = 0; i < 4u; ++i) {
        uint32_t child = guest(0xa46ccu, names[i], 0u, 0u);
        guest(0xa4b80u, EF_U8(child + 0x10u), 0u, 0u);
    }
}

/* Pilot 2/3/4 callbacks are three distinct named functions. The only
 * difference in their selected branch is the 67-byte row offset, slot,
 * widget and per-row fallback. Original guest calls remain visible. */
static void click_pilot(ef_addr32 widget, uint32_t row,
                        uint32_t slot, uint32_t selected, uint32_t other)
{
    if (guest(0xa4380u, widget + 0x3fu, 0x180914u, 0u) == 0u) {
        guest(0x779f4u, EF_U32(0x197668u) + row, slot, 0u);
        guest(0xbc38u, selected, slot, 0u);
    } else {
        uint32_t child = guest(0xa46ccu, other, 0u, 0u);
        guest(0xa4b80u, EF_U8(child + 0x10u), 0u, 0u);
    }
}

/* 0xbe48, 67 bytes: first slot explicitly zeroes EDX before 0xbc38;
 * 0x779f4 itself receives the post-strcmp guest EDX, not proven zero. */
void gui_click_pilot2_be48(ef_addr32 widget)
{
    click_pilot(widget, 0u, 0u, 0x1808f4u, 0x180918u);
}

/* 0xbe8c, 73 bytes: second row, EDX=1 at 0x779f4. */
void gui_click_pilot3_be8c(ef_addr32 widget)
{
    click_pilot(widget, 0x43u, 1u, 0x1808fcu, 0x180924u);
}

/* 0xbed8, 75 bytes: third row, EDX=2. */
void gui_click_pilot4_bed8(ef_addr32 widget)
{
    click_pilot(widget, 0x86u, 2u, 0x180904u, 0x18092cu);
}

/* 0xbf24, 75 bytes: fourth row, EDX=3. */
void gui_save_conflict_bf24(ef_addr32 widget)
{
    click_pilot(widget, 0xc9u, 3u, 0x18090cu, 0x180934u);
}

/* 0xca40 (25 bytes): width/height pair copied in reverse address order. */
void gui_alter_size_ca40(void)
{
    EF_U16(0x1a4ba2u) = EF_U16(0x1a365eu);
    EF_U16(0x1a4ba0u) = EF_U16(0x1a3660u);
}
#endif
