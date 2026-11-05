/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x69354, 29 bytes: exactly 13 pylon-related guest callbacks. The
 * original DX counts 0..12 and is preserved across the callee. */
void gpw_check_hit_pylon_69354(void)
{
    uint32_t index;
    for (index = 0u; index < 13u; ++index)
        gpw_ui_guest_call_085(0x68c18u, 0u, index);
}

/* 0x69914, 10 bytes: latch the current weapon visibility. */
void gpw_show_current_69914(void)
{
    EF_U16(0x12830eu) = 1u;
}

/* Original bit tests: low 0x10 OR high 0x04 increments the upper byte;
 * low 0x10 OR high 0x40 subtracts 0x600 from the 16-bit angle. */
static void rotate_view(ef_addr32 widget, uint32_t word_addr)
{
    uint16_t flags = EF_U16(widget + 0xcu);
    if ((flags & (0x0010u | 0x0400u)) != 0u)
        EF_U8(word_addr + 1u) = (uint8_t)(EF_U8(word_addr + 1u) + 6u);
    if ((flags & (0x0010u | 0x4000u)) != 0u)
        EF_U16(word_addr) = (uint16_t)(EF_U16(word_addr) - 0x600u);
}

/* 0x69920, 86 bytes: rotate one aircraft axis. */
void gpw_turn_y_plane_69920(ef_addr32 widget)
{
    rotate_view(widget, 0x1f6f3cu);
}

/* 0x69978, 89 bytes: rotate the other axis. */
void gpw_arcade_accepted_69978(ef_addr32 widget)
{
    rotate_view(widget, 0x1f6f3eu);
}

/* 0x699d4, 29 bytes: clear selected weapon UI text, not the data record. */
void gpw_open_big_current_weapon_699d4(ef_addr32 widget)
{
    uint32_t item = gpw_ui_guest_call_085(0xa78bcu, widget, 0x18630cu);
    ef_addr32 text = EF_U32(item + 0x27u);
    EF_U16(0x1f6f4au) = 0u;
    EF_U8(text) = 0u;
}
