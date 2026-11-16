/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xe284c, 106 bytes. EAX text, DX style, EAX width; EDX scratch.
 * EBX/ECX/ESI/EDI/EBP preserved. The sole caller a63d4 adds eight
 * pixels before storing the widget width. Style indexing is unchecked. */
uint32_t text_width_e284c(ef_addr32 text, uint16_t font)
{
    uint32_t total = 0;
    uint32_t code, width, slot, glyph_base;

    if (text == 0u)
        return 0;
    while (EF_U8(text) != 0u) {
        code = EF_U8(text);
        glyph_base = (uint32_t)font << 11;
        slot = (uint32_t)font * 10u;
        width = EF_U8(0x004198d2u + glyph_base + code * 8u);
        if (width != 0u)
            total += width + EF_U8(0x001378dau + slot);
        else if (code == 32u)
            total += EF_U8(0x001378d9u + slot);
        text++;
    }
    return total;
}
