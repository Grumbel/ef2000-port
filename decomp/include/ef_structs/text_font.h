/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_TEXT_FONT_H
#define EF_STRUCTS_TEXT_FONT_H

#include "ef_types.h"

#define EF_TEXT_FONT_TABLE   0x001378d8u /* ten-byte font records */
#define EF_TEXT_GLYPH_TABLE  0x004198d0u /* 256 eight-byte glyphs per font */
#define EF_TEXT_GLYPHS_PER_FONT 256

typedef struct EF_PACKED ef_text_font {
    uint8_t margin;                  /* +0, extent margin used by 0xe2564 */
    uint8_t space_advance;           /* +1, advance of a space */
    uint8_t glyph_spacing;           /* +2, added to every glyph width */
    uint8_t line_advance;            /* +3 */
    uint8_t unknown_04;
    ef_addr32 bitmap;                /* +5, glyph sheet */
    uint8_t format;                  /* +9, 1/2 sheet cells, 3 packed glyphs */
} ef_text_font;
EF_STATIC_ASSERT(ef_text_font_size, sizeof(ef_text_font) == 10);
EF_STATIC_ASSERT(ef_text_font_bitmap_off, offsetof(ef_text_font, bitmap) == 5);

typedef struct EF_PACKED ef_text_glyph {
    uint8_t sheet_x;                 /* +0, cell position in a sheet font */
    uint8_t sheet_y;                 /* +1 */
    uint8_t width;                   /* +2, 0 = no glyph */
    uint8_t height;                  /* +3 */
    uint32_t offset;                 /* +4, start in a packed font */
} ef_text_glyph;
EF_STATIC_ASSERT(ef_text_glyph_size, sizeof(ef_text_glyph) == 8);

#endif
