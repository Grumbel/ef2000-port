/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
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

/* 0x4cf4 (86 bytes): signed high-half difficulty index with the original
 * arithmetic 76-byte stride; only AX of the parsed integer is stored. */
void gui_change_difficulty_4cf4(ef_addr32 widget)
{
    uint32_t parsed = guest(0x9d4cdu, widget + 0x3fu, 0u, 0u);
    uint32_t target, row;
    int32_t selection;
    EF_U16(0x121e46u) = (uint16_t)parsed;
    target = guest(0xa46ccu, 0x17f7a4u, 0u, 0u);
    selection = (int32_t)EF_U32(0x121e44u) >> 16;
    row = (uint32_t)(selection - 1) * 76u + EF_U32(0x121e40u) + 0x28u;
    guest(0x7c4fcu, row, target, 0u);
    guest(0xa5ef8u, EF_U8(target + 0x10u), target, 0u);
}

/* 0xa960 (19 bytes): both alternatives tail-jump out of this function. */
void gui_change_mode_a960(ef_addr32 widget)
{
    guest(EF_U16(0x121e8eu) ? 0xa870u : 0x4595cu, widget, 0u, 0u);
}

/* 0xa974 (49 bytes): subtraction, not Boolean negation; non-Boolean
 * starting values remain distinguishable in the original 16-bit word. */
void gui_zoom_map_a974(ef_addr32 widget)
{
    uint16_t mode = (uint16_t)(1u - EF_U16(0x121e8eu));
    EF_U16(0x121e8eu) = mode;
    guest(0xa7588u, widget, mode ? 0x132119u : 0x13211eu, 0u);
}

/* 0xb574 (89 bytes): yes, the original repeats its comparison against the
 * *same* pointer 0x180834; no assumed second tab name is substituted. */
void gui_close_target_view_b574(ef_addr32 widget)
{
    uint32_t text = widget + 0x3fu;
    uint32_t target;
    if (guest(0xa4380u, text, 0x180834u, 0u) != 0u)
        EF_U8(0x121e95u) = 2u;
    else if (guest(0xa4380u, text, 0x180834u, 0u) != 0u)
        EF_U8(0x121e95u) = 1u;
    else
        EF_U8(0x121e95u) = 0u;
    target = guest(0xa46ccu, 0x1805f0u, 0u, 0u);
    guest(0xa5ef8u, EF_U8(target + 0x10u), 0u, 0u);
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

/* 0xc838 (48 bytes): signed word comparison after 32-bit INC EDX. */
void gui_show_debrief_c838(void)
{
    uint16_t next = (uint16_t)(EF_U16(0x121e98u) + 1u);
    EF_U16(0x121e98u) = next;
    if ((int16_t)next >= EF_S16(0x1a53c0u))
        EF_U16(0x121e98u) = 0u;
    guest(0xc868u, 0u, 0u, 0u);
}

/* 0xca40 (25 bytes): width/height pair copied in reverse address order. */
void gui_alter_size_ca40(void)
{
    EF_U16(0x1a4ba2u) = EF_U16(0x1a365eu);
    EF_U16(0x1a4ba0u) = EF_U16(0x1a3660u);
}
