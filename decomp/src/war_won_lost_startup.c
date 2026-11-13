/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x42b04, 131 bytes. WarWonLostStartup(EAX = map argument): if the
 * refresh flag 0x12654c or 0x1a559e is set, redraw the active marker and
 * update the map with the argument, then clear 0x12654c. Afterwards either
 * reset the counter at 0x12661c (when the signed high word of 0x12661a
 * exceeds 12, or the two word pairs at 0x1a4b9e..0x1a4ba4 match; the
 * 0x121e8c flag then triggers the 0x456e8 refresh) or increment it. */
void WarWonLostStartup(uint32_t arg)
{
    if (EF_U8(0x0012654cu) != 0u || EF_U8(0x001a559eu) != 0u) {
        draw_active_marker_e5088();
        guest_fn_426a4(arg);
        EF_U8(0x0012654cu) = 0u;
    }
    if (EF_S32(0x0012661au) >> 16 > 12 ||
        (EF_U16(0x001a4ba2u) == EF_U16(0x001a4ba4u) &&
         EF_U16(0x001a4ba0u) == EF_U16(0x001a4b9eu))) {
        EF_U16(0x0012661cu) = 0u;
        if (EF_U8(0x00121e8cu) != 0u)
            guest_fn_456e8();
    } else {
        EF_U16(0x0012661cu) = (uint16_t)(EF_U16(0x0012661cu) + 1u);
    }
}
