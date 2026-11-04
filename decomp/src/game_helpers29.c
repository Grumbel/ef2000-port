/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table reset, buffer and glyph-row helpers, scattered. */

/* 0x92f0, 69 bytes. Clear five 8-entry word tables. */
void clear_word_tables_92f0(void)
{
    int16_t i;

    for (i = 0; i < 8; i++) {
        uint32_t offset = (uint32_t)i * 2u;

        EF_U16(0x00197330u + offset) = 0;
        EF_U16(0x00197300u + offset) = 0;
        EF_U16(0x00197340u + offset) = 0;
        EF_U16(0x00197360u + offset) = 0;
        EF_U16(0x00197350u + offset) = 0;
    }
}

/* 0x56ccc, 73 bytes. Send the stored chat line as packet 0x2f when chat is allowed. */
void send_chat_line_56ccc(void)
{
    uint8_t packet[0x68];
    uint32_t i;

    if (EF_U8(0x001357c8u) == 0)
        return;
    if (guest_fn_b1b8b(0x001971b4u) == 0)
        return;
    if (EF_U16(0x001a662eu) == 0)
        return;
    for (i = 0; i < sizeof(packet); i++)
        packet[i] = 0;
    packet[0] = 0x2f;
    for (i = 0; i < 0x64; i++) {
        uint8_t c = EF_U8(0x001971b4u + i);

        packet[1 + i] = c;
        if (c == 0)
            break;
    }
    emit_command_packet(packet, 0x65);
}

/* 0x73924, 63 bytes. Release a unit's current entry and clear its pending slot id if it matches. */
void release_pending_73924(int16_t unit_id, int16_t slot_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);

    if (unit == 0)
        return;
    if ((uint16_t)unit_id == EF_U16(0x001f71cau))
        release_entry_65360(1, (uint16_t)slot_id);
    if ((uint16_t)slot_id == EF_U16(unit + 0x143u))
        EF_U16(unit + 0x143u) = 0xff;
}

/* 0x81868, 62 bytes. Fill 50 word pairs with generator output. */
void fill_random_pairs_81868(void)
{
    int16_t i;

    for (i = 0; i < 0x32; i++) {
        step_generator_a45b3();
        EF_U16(0x001fca10u + (uint32_t)i * 4u) = EF_U16(0x000a45a6u);
        step_generator_a45b3();
        EF_U16(0x001fca12u + (uint32_t)i * 4u) = EF_U16(0x000a45a6u);
    }
}

/* 0x90c2c, 64 bytes. Raise a 48-byte record's gauge by 0x41a, capped at 0x1400. */
void raise_gauge_90c2c(int16_t index)
{
    ef_addr32 address = 0x00211b02u + (uint32_t)((int32_t)index * 48);
    int32_t value = EF_S32(address);

    if (value < 0x1400) {
        value += 0x41a;
        EF_S32(address) = value;
        if (value > 0x1400)
            EF_S32(address) = 0x1400;
    }
}

/* 0xa0b98, 73 bytes. Copy one byte between two page rows (29-byte stride, offset 0x14). */
void copy_page_byte_a0b98(int16_t from, int16_t row, int16_t to)
{
    ef_addr32 dest;
    ef_addr32 source;

    if (EF_U16(0x00132ad2u) == 0xffffu)
        return;
    dest = (ef_addr32)((int32_t)row * 29) + EF_U32(0x00417174u) + 0xbu;
    source = guest_fn_b3f58((int32_t)EF_U32(0x00132ad0u) >> 16) + 0xbu + (uint32_t)(int32_t)to;
    EF_U8(dest + (uint32_t)(int32_t)from + 0x14u) = EF_U8(source + 0x14u);
}

/* 0xa4b08, 66 bytes. Refresh the list for an object that is flagged and not owned. */
void refresh_flagged_object_a4b08(ef_addr32 object)
{
    if (object == 0)
        return;
    if (EF_U16(0x0041eda8u) == 0 || EF_U16(0x0041edaau) == 0)
        return;
    if (EF_U8(object + 0x11u) != 0)
        return;
    if ((EF_U8(object + 0x3bu) & 4u) == 0)
        return;
    guest_fn_a4a94(EF_U8(object + 0x10u));
    guest_fn_a499c(EF_U8(object + 0x10u));
    guest_fn_a6038();
}

/* 0xdf2b0, 51 bytes. Divide 115200 by the table entry for the current rate index. */
int32_t rate_divisor_df2b0(void)
{
    uint32_t index = EF_U8(0x0015f07du);

    if (index >= 8)
        return 0;
    return (int32_t)0x1c200 / (int32_t)EF_U16(0x000df290u + index * 2u);
}

/* 0xe3b58, 34 bytes. Release the saved handle, if any. */
void release_saved_handle_e3b58(void)
{
    int32_t handle = EF_S32(0x0015f8fcu);

    if (handle != -1) {
        guest_fn_a7990((uint32_t)handle);
        EF_S32(0x0015f8fcu) = -1;
    }
}

/* 0xf2141, 66 bytes. Advance a window cursor by count; wrap and report 1 when out of range. */
uint32_t advance_window_f2141(int32_t count, ef_addr32 window)
{
    EF_U8(window + 0xcu) = (uint8_t)(EF_U8(window + 0xcu) & 0xefu);
    if (count <= EF_S32(window + 4u) &&
        count >= EF_S32(window + 8u) - EF_S32(window)) {
        int32_t remaining = EF_S32(window + 4u) - count;

        EF_S32(window) = EF_S32(window) + count;
        EF_S32(window + 4u) = remaining;
        return 0;
    }
    EF_S32(window + 4u) = 0;
    EF_U32(window) = EF_U32(window + 8u);
    return 1;
}

/* 0xf261c, 61 bytes. Draw a string with 4-pixel glyph spacing. */
void draw_text_f261c(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text)
{
    if (EF_U8(text) == 0)
        return;
    do {
        uint32_t glyph = EF_U8(text) & 0x3fu;

        text++;
        guest_fn_f25b0(color, x, y, glyph);
        x += 4u;
    } while (EF_U8(text) != 0);
}

/* 0xf29ec, 57 bytes. Draw a string with 6-pixel spacing, glyphs offset from the blank. */
void draw_text_f29ec(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text)
{
    if (EF_U8(text) == 0)
        return;
    do {
        uint32_t glyph = (uint32_t)EF_U8(text) - 0x20u;

        text++;
        guest_fn_f2960(color, x, y, glyph);
        x += 6u;
    } while (EF_U8(text) != 0);
}

/* 0xf41a8, 27 bytes. With the flag set, remember the request value and refresh the cursor. */
void remember_request_f41a8(void)
{
    if (EF_U8(0x00137206u) == 0)
        return;
    EF_U32(0x00161e2cu) = (uint32_t)((int32_t)EF_U32(0x00426124u) >> 16);
    guest_fn_de4b0();
}

/* 0xf41c4, 22 bytes. With the flag set, apply the saved request value. */
void apply_request_f41c4(void)
{
    if (EF_U8(0x00137206u) == 0)
        return;
    guest_fn_de3b4((int32_t)EF_U32(0x00161e2au) >> 16);
}

/* 0xf5e6c, 61 bytes. Latch six words and install the 0xf5eac handler. */
void latch_words_f5e6c(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint32_t stack_e, uint32_t stack_f)
{
    EF_U16(0x00485de8u) = a;
    EF_U16(0x00485deau) = b;
    EF_U16(0x00485decu) = c;
    EF_U16(0x00485deeu) = d;
    EF_U16(0x00485df0u) = (uint16_t)stack_e;
    EF_U32(0x00161ea4u) = 0x000f5eacu;
    EF_U16(0x00485df2u) = (uint16_t)stack_f;
}
