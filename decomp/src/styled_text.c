/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xe26bc, 400 bytes. EAX unused, EDX X, BX Y, ECX text,
 * stack style byte; RET4. ESI/EDI/EBP preserved. Caller a6cdb passes
 * EAX zero and retains its widget pointer across the draw. */
void draw_styled_text_e26bc(uint32_t unused, uint32_t x, uint32_t y,
                           ef_addr32 text, uint32_t style)
{
    uint32_t style_index, mode, slot, glyph_base, code, width, height, source_x, source_y;
    uint32_t advance_width, spacing, span;
    ef_addr32 glyph, bitmap;
    (void)unused;
    style_index = (uint8_t)style;
    mode = EF_U8(0x001378e1u + style_index * 10u);
    x += 4u;
    if (mode < 1u || mode > 3u)
        return;
    while (EF_U8(text) != 0u) {
        /* Recompute per character, as in both original loops. */
        slot = style_index * 10u;
        glyph_base = style_index << 11;
        code = EF_U8(text);
        glyph = 0x004198d0u + glyph_base + code * 8u;
        width = EF_U8(glyph + 2u);
        if (width != 0u) {
            if (mode < 3u) {
                /* The first width load only gates the draw; reload it
                 * after height, matching the original argument loads. */
                span = 256;
                height = EF_U8(glyph + 3u);
                width = EF_U8(glyph + 2u);
                source_y = EF_U8(glyph + 1u);
                source_x = EF_U8(glyph);
            } else {
                span = width;
                height = EF_U8(glyph + 3u);
                source_x = 0;
                source_y = 0;
            }
            bitmap = EF_U32(0x001378ddu + slot);
            guest_blit_clip_ca290(bitmap, 0x0041ec08u, source_x, source_y,
                                  width, height, (uint32_t)(int32_t)(int16_t)x,
                                  (uint32_t)(int32_t)(int16_t)y, span);
            /* Drawing may change the text byte and glyph metrics. */
            code = EF_U8(text);
            advance_width = EF_U8(0x004198d2u + glyph_base + code * 8u);
            spacing = EF_U8(0x001378dau + slot);
            x += advance_width + spacing;
        } else if (code == 32u) {
            x += EF_U8(0x001378d9u + slot);
        }
        text++;
    }
}
