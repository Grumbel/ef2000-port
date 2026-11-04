/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Stream, glyph and column-scan helpers, 0xdf21b..0xe3908. */

/* 0xdf21b, 41 bytes. Read the next byte from a stream, or -1 (setting flag 2) at its end. */
int32_t read_stream_byte_df21b(ef_addr32 stream)
{
    ef_addr32 cursor = EF_U32(stream + 8u);
    uint32_t value = EF_U8(cursor);

    if (value != 0) {
        EF_U32(stream + 8u) = cursor + 1u;
        return (int32_t)value;
    }
    EF_U8(stream + 0x10u) = (uint8_t)(EF_U8(stream + 0x10u) | 0x02u);
    return -1;
}

/* 0xe17b8, 64 bytes. True when a column of 256-byte rows is clear for the given count. */
uint8_t rows_clear_e17b8(uint32_t column, int32_t first_row, int32_t count)
{
    ef_addr32 plane = EF_U32(0x0041eea8u);
    int32_t row;
    ef_addr32 cell = (ef_addr32)(first_row << 8) + column;

    for (row = first_row; row < first_row + count; row++) {
        if (EF_U8(cell + plane) != 0)
            return 0;
        cell += 0x100u;
    }
    return 1;
}

/* 0xe3040, 160 bytes. Draw a 8x6 glyph, each set bit writes the colour byte. */
void draw_glyph_e3040(uint32_t color, uint32_t x, uint32_t y, uint32_t glyph)
{
    uint32_t stride = EF_U32(0x001631f4u);
    ef_addr32 dest = EF_U32(0x0015b754u) + EF_U32(0x00163200u + y * 4u) + x;
    ef_addr32 bits = 0x0015f770u + glyph * 6u;
    int32_t row;

    for (row = 0; row < 6; row++) {
        uint32_t mask = EF_U8(bits);
        int32_t bit;

        bits++;
        for (bit = 0; bit < 8; bit++) {
            if (mask & (0x80u >> bit))
                EF_U8(dest + (uint32_t)bit) = (uint8_t)color;
        }
        dest += stride;
    }
    EF_U32(0x001631f4u) = stride;
}

/* 0xe30e0, 61 bytes. Draw a string with 9-pixel glyph spacing. */
void draw_text_e30e0(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text)
{
    if (EF_U8(text) == 0)
        return;
    do {
        uint32_t glyph = EF_U8(text) & 0x3fu;

        text++;
        draw_glyph_e3040(color, x, y, glyph);
        x += 9u;
    } while (EF_U8(text) != 0);
}

/* 0xe3908, 51 bytes. Search a word table backwards for a value, using slot 0 as the sentinel. */
uint32_t find_word_backwards_e3908(uint32_t value)
{
    ef_addr32 table = EF_U32(0x00429224u);
    uint32_t index = value;

    EF_U16(table) = (uint16_t)value;
    index = (index & 0xffff0000u) | EF_U16(0x0015f902u);
    do {
        index--;
    } while (EF_U16(table + (uint32_t)((int32_t)(int16_t)index * 2)) != (uint16_t)value);
    EF_U32(0x00429224u) = table;
    return index;
}
