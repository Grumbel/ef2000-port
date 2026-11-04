/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Proportional font drawing and measuring, plus constant and slope helpers, 0xdccd8..0xe85f0. */

/* 0xdccd8, 98 bytes. Draw a string in the 4-pixel-blank font, advancing by each glyph width. */
void draw_text_4_dccd8(uint8_t color, int32_t x, uint16_t y, ef_addr32 text)
{
    int16_t i;

    for (i = 0;; i++) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);
        uint32_t glyph;
        uint32_t advance;

        if (c == 0)
            return;
        if (c == 0x20u || c == 0x09u) {
            x += 4;
            continue;
        }
        glyph = (uint32_t)to_upper_ascii((int32_t)c) & 0xffu;
        advance = guest_fn_dcc98(glyph, (int32_t)(int16_t)x, (int32_t)(int16_t)y, color) & 0xffu;
        x += (int32_t)advance;
    }
}

/* 0xdd1b0, 95 bytes. Draw a string in the 5-pixel-blank font, advancing by each glyph width. */
void draw_text_5_dd1b0(uint8_t color, int32_t x, uint16_t y, ef_addr32 text)
{
    int16_t i;

    for (i = 0;; i++) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);
        uint32_t glyph;
        uint32_t advance;

        if (c == 0)
            return;
        if (c == 0x20u || c == 0x09u) {
            x += 5;
            continue;
        }
        glyph = (uint32_t)to_upper_ascii((int32_t)c) & 0xffu;
        advance = guest_fn_dd164(glyph, (int32_t)(int16_t)x, (int32_t)(int16_t)y, color) & 0xffu;
        x += (int32_t)advance;
    }
}

/* Shared body of the three table-driven width functions: blank width, table, gap, final trim. */
static uint32_t measure_text_table(ef_addr32 text, uint32_t blank, ef_addr32 table, uint32_t gap, uint32_t trim)
{
    uint32_t width = 0;
    int16_t i;

    for (i = 0;; i++) {
        uint8_t c = EF_U8(text + (uint32_t)(int32_t)i);
        int32_t glyph;
        uint32_t w;

        if (c == 0)
            return width - trim;
        if (c == 0xa5u)
            c = 0x4e;
        if (c == 0x20u || c == 0x09u) {
            width += blank;
            continue;
        }
        glyph = (int8_t)EF_U8(0x0015e1beu + (c & 0x3fu));
        /* ORIGINAL BUG: a missing glyph (-1) leaves the high word set in the width. */
        w = ((uint32_t)glyph & 0xffff0000u) | EF_U8(table + (uint32_t)glyph);
        width += w + gap;
    }
}

/* 0xdd460, 100 bytes. Width of a string in the 17-pixel-blank font minus the trailing gap. */
uint32_t text_width_17_dd460(ef_addr32 text)
{
    return measure_text_table(text, 0x11u, 0x0015e27eu, 2u, 2u);
}

/* 0xdd55c, 94 bytes. Width of a string in the 5-pixel-blank font minus the trailing gap. */
uint32_t text_width_5_dd55c(ef_addr32 text)
{
    return measure_text_table(text, 5u, 0x0015e2a2u, 1u, 1u);
}

/* 0xdd728, 94 bytes. Width of a string in the 6-pixel-blank font minus the trailing gap. */
uint32_t text_width_6_dd728(ef_addr32 text)
{
    return measure_text_table(text, 6u, 0x0015e2eau, 2u, 2u);
}

/* 0xe7850, 97 bytes. Store the double constants used by the time conversion. */
void init_time_constants_e7850(void)
{
    EF_U32(0x00432da8u) = 0x9999999au;
    EF_U32(0x00432dacu) = 0x3ff39999u;
    EF_U32(0x00432dd8u) = 0x66666666u;
    EF_U32(0x00432ddcu) = 0x40720266u;
    EF_U32(0x00432dc8u) = 0x76c8b439u;
    EF_U32(0x00432dccu) = 0x3f7a9fbeu;
    EF_U32(0x00432dd0u) = 0;
    EF_U32(0x00432dd4u) = 0x40c57c00u;
}

/* 0xe85f0, 97 bytes. Interpolate a point back along a slope defined by the line at 0x420904. */
void interpolate_line_e85f0(ef_addr32 out_x, ef_addr32 out_y)
{
    int32_t span;

    EF_U32(out_x) = EF_U32(0x00420910u);
    EF_U32(out_y) = EF_U32(0x00420918u);
    span = (int32_t)(EF_U32(0x00420908u) - EF_U32(0x00420904u));
    if (span == 0)
        return;
    EF_S32(out_x) = EF_S32(out_x) -
        (int32_t)(((int64_t)(int32_t)(EF_U32(0x00420914u) - EF_U32(0x00420910u)) *
                   (int64_t)(int32_t)EF_U32(0x00420904u)) / span);
    EF_S32(out_y) = EF_S32(out_y) -
        (int32_t)(((int64_t)(int32_t)(EF_U32(0x0042091cu) - EF_U32(0x00420918u)) *
                   (int64_t)(int32_t)EF_U32(0x00420904u)) / span);
}
