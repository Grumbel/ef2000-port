/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/text_font.h"

#define TEXT_CLIP_RECT     0x0041ec08u
#define SHEET_STRIDE       0x100u
#define FONT_FORMAT_PACKED 3u

static ef_addr32 font_record(uint32_t font)
{
    return EF_TEXT_FONT_TABLE + font * sizeof(ef_text_font);
}

static ef_addr32 glyph_record(uint32_t font, uint32_t ch)
{
    return EF_TEXT_GLYPH_TABLE + (font * EF_TEXT_GLYPHS_PER_FONT + ch) * sizeof(ef_text_glyph);
}

/* Width of a glyph plus the font spacing, or the space advance for a
 * blank space; other empty glyphs do not advance. */
static uint32_t glyph_advance(uint32_t font, uint32_t ch)
{
    uint32_t width = EF_U8(glyph_record(font, ch) + offsetof(ef_text_glyph, width));

    if (width != 0u)
        return width + EF_U8(font_record(font) + offsetof(ef_text_font, glyph_spacing));
    if (ch == 0x20u)
        return EF_U8(font_record(font) + offsetof(ef_text_font, space_advance));
    return 0u;
}

/* 0xe2374, 493 bytes. EAX view, EDX text node (x, y words, length byte
 * +4, text pointer +6). Draw the node's text clipped to 0x41ec08 with the
 * view's font ((+0xcb >> 1) & 15). The position is relative to the
 * view's origin (+0x41/+0x43 plus the size +0x71/+0x73 minus the scroll
 * +0x69/+0x6b), in 16-bit arithmetic. Sheet fonts (format 1 and 2) copy
 * each glyph cell from the 256-pixel-wide sheet; packed fonts (3) copy
 * the glyph from its own offset with its width as the stride. Other
 * formats draw nothing. EDX is clobbered. Caller 0xe2564. */
void draw_text_node_e2374(ef_addr32 view, ef_addr32 node)
{
    uint32_t font = (EF_U8(view + 0xcbu) >> 1) & 0x0fu;
    int16_t x = (int16_t)(EF_U16(view + 0x41u) + EF_U16(view + 0x71u) - EF_U16(view + 0x69u) + EF_U16(node));
    int16_t y = (int16_t)(EF_U16(view + 0x43u) + EF_U16(view + 0x73u) - EF_U16(view + 0x6bu) + EF_U16(node + 2u));
    uint8_t count = EF_U8(node + 4u);
    ef_addr32 text = EF_U32(node + 6u);
    ef_addr32 font_bitmap;
    uint8_t format;
    int packed;

    if (count == 0u)
        return;
    format = EF_U8(font_record(font) + offsetof(ef_text_font, format));
    if (format < 1u || format > FONT_FORMAT_PACKED)
        return;
    packed = format == FONT_FORMAT_PACKED;

    for (; count != 0u; --count, ++text) {
        uint32_t ch = EF_U8(text);
        ef_addr32 glyph = glyph_record(font, ch);
        uint32_t width = EF_U8(glyph + offsetof(ef_text_glyph, width));
        uint32_t height = EF_U8(glyph + offsetof(ef_text_glyph, height));

        if (width != 0u) {
            font_bitmap = EF_U32(font_record(font) + offsetof(ef_text_font, bitmap));
            if (packed)
                blit_bitmap_clipped_ca290(font_bitmap + EF_U32(glyph + offsetof(ef_text_glyph, offset)),
                                          TEXT_CLIP_RECT, 0u, 0u, width, height,
                                          (uint32_t)(int32_t)x, (uint32_t)(int32_t)y, width);
            else
                blit_bitmap_clipped_ca290(font_bitmap, TEXT_CLIP_RECT,
                                          EF_U8(glyph + offsetof(ef_text_glyph, sheet_x)),
                                          EF_U8(glyph + offsetof(ef_text_glyph, sheet_y)),
                                          width, height,
                                          (uint32_t)(int32_t)x, (uint32_t)(int32_t)y, SHEET_STRIDE);
        }
        /* The advance re-reads the glyph after the blit, as the original does. */
        x = (int16_t)(x + (int16_t)glyph_advance(font, EF_U8(text)));
    }
}
