/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint16_t top, left;
    uint8_t reserved[0x0a];
    uint8_t shadow, progressive;
    uint8_t font, color;
    uint16_t length;
    ef_addr32 text;
} ef_scrolling_text_prefix;

EF_STATIC_ASSERT(scrolling_text_top, offsetof(ef_scrolling_text_prefix, top) == 0);
EF_STATIC_ASSERT(scrolling_text_left, offsetof(ef_scrolling_text_prefix, left) == 2);
EF_STATIC_ASSERT(scrolling_text_shadow, offsetof(ef_scrolling_text_prefix, shadow) == 0x0e);
EF_STATIC_ASSERT(scrolling_text_progressive, offsetof(ef_scrolling_text_prefix, progressive) == 0x0f);
EF_STATIC_ASSERT(scrolling_text_font, offsetof(ef_scrolling_text_prefix, font) == 0x10);
EF_STATIC_ASSERT(scrolling_text_color, offsetof(ef_scrolling_text_prefix, color) == 0x11);
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

static void draw_scrolling_line(ef_addr32 text, uint32_t line, int32_t top)
{
    if (EF_U8(EF_U32(0x004191d0u) + 0x0eu) != 0u) {
        draw_mode_dispatch_a8660(text,
            (uint32_t)((int32_t)EF_S16(0x004194b2u) + 1),
            (uint32_t)(top + 1), 0u, (uint32_t)(int32_t)(int16_t)line);
    }
    draw_mode_dispatch_a8660(text, (uint32_t)(int32_t)EF_S16(0x004194b2u),
        (uint32_t)top, EF_U8(EF_U32(0x004191d0u) + 0x11u),
        (uint32_t)(int32_t)(int16_t)line);
}

/* Original local frame: the partial line occupies the first 80 bytes.
 * Keep the following live slots at their original offsets: unchecked
 * partial-line writes can overwrite them. The high word at +5a is unused. */
typedef struct {
    uint8_t partial[0x50];
    uint32_t line_offset, line_index;
    uint16_t top, unused;
} ef_scrolling_draw_frame;

EF_STATIC_ASSERT(scrolling_draw_line_offset, offsetof(ef_scrolling_draw_frame, line_offset) == 0x50);
EF_STATIC_ASSERT(scrolling_draw_line_index, offsetof(ef_scrolling_draw_frame, line_index) == 0x54);
EF_STATIC_ASSERT(scrolling_draw_top, offsetof(ef_scrolling_draw_frame, top) == 0x58);
EF_STATIC_ASSERT(scrolling_draw_frame_size, sizeof(ef_scrolling_draw_frame) == 0x5c);

/* 0xa8718, 574 bytes. No register inputs: the record is read from 4191d0.
 * All GPRs except EAX are preserved. Render complete lines or reveal a
 * clock-paced prefix; the last prefix is drawn even when it is empty. */
void draw_scrolling_text_a8718(void)
{
    uint32_t line = 0u;
    uint32_t top = EF_U16(0x004194b6u);

    if (EF_U8(EF_U32(0x004191d0u) + 0x0fu) == 0u) {
        while ((int16_t)line < EF_S16(0x004194b8u)) {
            draw_scrolling_line(0x004187f8u + (uint32_t)((int32_t)(int16_t)line * 0x50),
                                line, (int32_t)(int16_t)top);
            line++;
            top += mode_code_a7dac();
        }
    } else {
        ef_scrolling_draw_frame frame;
        ef_addr32 local = (ef_addr32)(uintptr_t)&frame;
        uint32_t remaining;
        uint32_t copied = 0u;
        uint32_t ticks;
        int32_t advance;

        clamp_word_425fa6();
        ticks = (uint32_t)clock_delta_a7d40();
        EF_U16(0x004194bau) = (uint16_t)(EF_U16(0x004194bau) + ticks);
        advance = EF_S16(0x004194bau) / 2;
        EF_U16(0x00425fa6u) = (uint16_t)(EF_U16(0x00425fa6u) + advance);
        EF_U16(0x004194bau) = (uint16_t)(EF_U16(0x004194bau) - advance * 2);
        if (EF_S16(0x00425fa6u) > EF_S16(0x004194aeu))
            EF_U16(0x00425fa6u) = EF_U16(0x004194aeu);
        remaining = EF_U16(0x00425fa6u);
        EF_U16(local + 0x58u) = EF_U16(0x004194b6u);

        while ((int16_t)remaining > 0 &&
               (int16_t)remaining >= EF_S16(0x004187a8u +
                   (uint32_t)((int32_t)(int16_t)line * 2))) {
            ef_addr32 text;
            uint32_t length;

            EF_U32(local + 0x54u) = (uint32_t)(int32_t)(int16_t)line;
            EF_U32(local + 0x50u) = (uint32_t)((int32_t)(int16_t)line * 0x50);
            text = 0x004187f8u + EF_U32(local + 0x50u);
            draw_scrolling_line(text, line, (int32_t)EF_S16(local + 0x58u));
            line++;
            length = EF_U16(0x004187a8u + EF_U32(local + 0x54u) * 2u);
            top = EF_U16(local + 0x58u);
            top += mode_code_a7dac();
            remaining -= length;
            EF_U16(local + 0x58u) = (uint16_t)top;
        }
        /* ORIGINAL BUG: unsigned comparison against a sign-extended word;
         * no capacity check on the 80-byte partial-line buffer. */
        while (copied < (uint32_t)(int32_t)(int16_t)remaining) {
            uint8_t c = EF_U8(0x004187f8u +
                (uint32_t)((int32_t)(int16_t)line * 0x50) + copied);
            copied++;
            EF_U8(local + copied - 1u) = c;
        }
        EF_U8(local + copied) = 0u;
        draw_scrolling_line(local, line, (int32_t)EF_S16(local + 0x58u));
    }
}
