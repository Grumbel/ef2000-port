/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint16_t x, y;
    uint8_t reserved[0x0c];
    uint8_t font, flags;
    uint16_t length;
    ef_addr32 text;
} ef_scrolling_text_prefix;

EF_STATIC_ASSERT(scrolling_text_x, offsetof(ef_scrolling_text_prefix, x) == 0);
EF_STATIC_ASSERT(scrolling_text_y, offsetof(ef_scrolling_text_prefix, y) == 2);
EF_STATIC_ASSERT(scrolling_text_font, offsetof(ef_scrolling_text_prefix, font) == 0x10);
EF_STATIC_ASSERT(scrolling_text_length, offsetof(ef_scrolling_text_prefix, length) == 0x12);
EF_STATIC_ASSERT(scrolling_text_text, offsetof(ef_scrolling_text_prefix, text) == 0x14);

/* 0xa82f0, 607 bytes. EAX=record; all other GPRs preserved. The CS jump
 * table at a82c8 selects the ten original font modes. No EAX result is
 * consumed by the callers at a82af/a8978/a8a6a/a8b1a/a8cb0/a8d99. */
void prepare_scrolling_text_a82f0(ef_addr32 record)
{
    ef_addr32 text = EF_U32(record + 0x14u);
    uint32_t position = 0u;
    uint32_t column = 0u;
    uint32_t line = 0u;

    EF_U32(0x004191d0u) = record;
    EF_U16(0x004194b6u) = EF_U16(record);
    EF_U16(0x004194b2u) = EF_U16(record + 2u);
    while ((int32_t)(int16_t)position <
           (int32_t)EF_U16(EF_U32(0x004191d0u) + 0x12u)) {
        ef_addr32 terminator;
        ef_addr32 output;
        uint8_t c;

        for (;;) {
            /* The original reads the byte before checking the length. */
            c = EF_U8(text + (uint32_t)(int32_t)(int16_t)position);
            if (c == '^' || c == '\n' || c == '\r' ||
                (int32_t)(int16_t)position >=
                (int32_t)EF_U16(EF_U32(0x004191d0u) + 0x12u))
                break;
            output = 0x004187f8u + (uint32_t)((int32_t)(int16_t)line * 0x50);
            c = (uint8_t)to_upper_ascii(c);
            position++;
            EF_U8(output + (uint32_t)(int32_t)(int16_t)column) = c;
            column++;
        }
        terminator = text + (uint32_t)(int32_t)(int16_t)position;
        if (EF_U8(terminator) == '^') {
            open_named_data_a81e4(terminator + 1u, record);
            return;
        }
        output = 0x004187f8u + (uint32_t)((int32_t)(int16_t)line * 0x50);
        /* ORIGINAL BUG: no line/column capacity checks before these stores. */
        EF_U8(output + (uint32_t)(int32_t)(int16_t)column) = 0u;
        EF_U16(0x004187a8u + (uint32_t)((int32_t)(int16_t)line * 2)) =
            (uint16_t)expand_placeholders_a80b0(output);
        c = EF_U8(terminator);
        line++;
        /* CR always skips two bytes, without checking for a following LF. */
        if (c == '\r')
            position += 2u;
        else if (c == '\n')
            position++;
        column = 0u;
    }

    EF_U16(0x004194b8u) = (uint16_t)line;
    EF_U16(0x004194aeu) = 0u;
    line = 0u;
    while ((int16_t)line < EF_S16(0x004194b8u)) {
        uint32_t offset = (uint32_t)((int32_t)(int16_t)line * 2);
        ef_addr32 output = 0x004187f8u + (uint32_t)((int32_t)(int16_t)line * 0x50);
        uint32_t width;
        uint8_t font;

        EF_U16(0x004194aeu) = (uint16_t)(EF_U16(0x004194aeu) +
                                              EF_U16(0x004187a8u + offset));
        font = EF_U8(EF_U32(0x004191d0u) + 0x10u);
        switch (font) {
        case 0: width = text_width_6_dd728(output); break;
        case 1:
        case 2:
            {
                int32_t scale = font == 1u ? 8 : 6;
                int32_t count = EF_S16(0x004187a8u + offset);

                EF_U16(0x004187d0u + offset) = (uint16_t)(160 - (count * scale) / 2);
                EF_U16(0x00418780u + offset) = (uint16_t)(EF_U16(0x004187a8u + offset) * scale);
            }
            line++;
            continue;
        case 3: width = text_width_4_dced0(output); break;
        case 4: width = text_width_6_dd210(output); break;
        case 5: width = text_width_5_dd10c(output); break;
        case 6: width = text_width_dd2f4(output); break;
        case 7: width = text_width_17_dd460(output); break;
        case 8: width = text_width_5_dd55c(output); break;
        case 9: width = text_width_raw_dd650(output); break;
        default:
            line++;
            continue;
        }
        EF_U16(0x00418780u + offset) = (uint16_t)width;
        EF_U16(0x004187d0u + offset) =
            (uint16_t)(160 - EF_S16(0x00418780u + offset) / 2);
        line++;
    }
    remember_clock_a7d34();
    EF_U16(0x004194bau) = 0u;
}
