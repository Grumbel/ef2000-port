/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rectangle copies between the screen and the backing buffer. */

/* 0x109e08, 55 bytes. Copy count rows of len bytes at row offset table[idx] + x from the screen (0x15b754) into the buffer returned by 0xc382c. */
void copy_rows_from_screen_109e08(uint32_t x, uint32_t idx, uint32_t len, int32_t count)
{
    uint32_t offset = EF_U32(0x00163200u + idx * 4u) + x;
    copy_rows_109dd0(guest_fn_c382c() + offset, EF_U32(0x0015b754u) + offset, len, count);
}

/* 0xe5114, 88 bytes. Copy every rectangle of the list at 0x43399e (8-byte entries, count in the word at 0x433eac) from the screen into the backing buffer. */
void save_rect_list_e5114(void)
{
    uint32_t i;
    for (i = 0; i < 0xffffu && (uint16_t)i < EF_U16(0x00433eacu); i++) {
        uint32_t e = 0x0043399eu + i * 8u;
        copy_rows_from_screen_109e08((uint32_t)(int32_t)EF_S16(e + 2u), (uint32_t)(int32_t)EF_S16(e + 4u),
                                     (uint32_t)(int32_t)EF_S16(e + 6u), (int32_t)EF_S16(e + 8u));
    }
}

/* 0xe516c, 88 bytes. Copy every rectangle of the list at 0x43399e from the backing buffer back to the screen. */
void restore_rect_list_e516c(void)
{
    uint32_t i;
    for (i = 0; i < 0xffffu && (uint16_t)i < EF_U16(0x00433eacu); i++) {
        uint32_t e = 0x0043399eu + i * 8u;
        copy_rows_at_109e40((uint32_t)(int32_t)EF_S16(e + 2u), (uint32_t)(int32_t)EF_S16(e + 4u),
                            (uint32_t)(int32_t)EF_S16(e + 6u), (int32_t)EF_S16(e + 8u));
    }
}

/* Draw a glyph from a font of 7-byte rows: each row byte's high seven bits select pixels set to the colour. */
static void blit_glyph_common(uint32_t x, uint32_t y, uint32_t glyph, uint32_t font, uint32_t rows, uint32_t colour_addr)
{
    uint8_t colour = EF_U8(colour_addr);
    uint32_t stride = EF_U32(0x001631f4u);
    uint32_t dst = x + EF_U32(0x0015b754u) + EF_U32(0x00163200u + y * 4u);
    uint32_t src = font + glyph * 7u;
    uint32_t row, bit;
    for (row = 0; row < rows; row++) {
        uint8_t bits = EF_U8(src++);
        for (bit = 0; bit < 7u; bit++) {
            if (bits & (0x80u >> bit))
                EF_U8(dst + bit) = colour;
        }
        dst += stride;
    }
    EF_U32(0x001631f4u) = stride;
    EF_U8(colour_addr) = colour;
}

/* 0xe5c30, 138 bytes. Draw a 7x7 glyph at (x, y). */
void draw_glyph_e5c30(uint32_t x, uint32_t y, uint32_t glyph)
{
    blit_glyph_common(x, y, glyph, 0x0015fab0u, 7u, 0x00433eb0u);
}

/* 0xf2320, 138 bytes. Draw a 7x6 glyph from the second font at (x, y). */
void draw_glyph_f2320(uint32_t x, uint32_t y, uint32_t glyph)
{
    blit_glyph_common(x, y, glyph, 0x00160b04u, 6u, 0x00484680u);
}
