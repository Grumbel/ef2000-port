/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xf25b0, 107 bytes. AL colour, EDX X, EBX row, ECX glyph;
 * ESI/EDI/EBP preserved, four scratch registers, plain RET.
 * Sole caller f2644 masks glyph to six bits and advances X by four. */
void draw_glyph_f25b0(uint8_t value, uint32_t x, uint32_t row, uint32_t glyph)
{
    uint32_t stride = EF_U32(0x001631f4u);
    ef_addr32 pixel = EF_U32(0x0015b754u);
    ef_addr32 source;
    uint32_t i;
    uint8_t bits;

    pixel += EF_U32(0x00163200u + row * 4u);
    pixel += x;
    source = 0x00160cc4u + glyph * 6u;
    for (i = 0; i < 6u; i++) {
        bits = EF_U8(source);
        source++;
        if ((bits & 4u) != 0u)
            EF_U8(pixel) = value;
        if ((bits & 2u) != 0u)
            EF_U8(pixel + 1u) = value;
        if ((bits & 1u) != 0u)
            EF_U8(pixel + 2u) = value;
        pixel += stride;
    }
    /* Preserve the original final write, even if pixels alias this field. */
    EF_U32(0x001631f4u) = stride;
}
