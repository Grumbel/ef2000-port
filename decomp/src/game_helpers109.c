/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Glyph dispatch with extended codes. */

/* 0xdcf28, 135 bytes. Draw the glyph for a character code (0xa5 maps to 0x4e, codes above 0x7f use the extended tables) through cf7c4; returns its advance (width + 2) or 0 when the code has no glyph. */
uint8_t draw_glyph_dcf28(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val)
{
    if (code == 0xa5)
        code = 0x4e;
    if (code > 0x7f) {
        uint32_t idx = (uint8_t)(code - 0x80);
        uint8_t width = 0;

        if (EF_U8(0x0015ec0cu + idx) != 0) {
            guest_fn_cf7c4_x(EF_U32(0x0015ea84u + idx * 4u), (uint32_t)(int32_t)bx_val, (uint32_t)(int32_t)dx_val, (uint32_t)(int32_t)cx_val);
            width = EF_U8(0x0015ec0cu + idx);
        }
        return (uint8_t)(width + 2u);
    } else {
        int8_t idx = (int8_t)EF_U8(0x0015e1beu + (uint32_t)(code & 0x3fu));

        if (idx == -1)
            return 0;
        guest_fn_cf7c4_x(EF_U32(0x0015e41cu + (uint32_t)(int32_t)idx * 4u), (uint32_t)(int32_t)bx_val, (uint32_t)(int32_t)dx_val, (uint32_t)(int32_t)cx_val);
        return (uint8_t)(EF_U8(0x0015e231u + (uint32_t)(int32_t)idx) + 2u);
    }
}
