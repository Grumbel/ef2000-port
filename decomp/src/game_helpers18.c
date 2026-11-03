/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Command-line tokeniser, message handlers and packet builders, 0x50d04..0x56444. */

static void put16(uint8_t *bytes, uint32_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t offset, uint32_t value)
{
    put16(bytes, offset, (uint16_t)value);
    put16(bytes, offset + 2, (uint16_t)(value >> 16));
}

static int is_separator_a(uint8_t c)
{
    return c == 0x20 || c == 0x28 || c == 0x09 || c == 0x2c || c == 0x29 || c == 0x23;
}

static int is_separator_b(uint8_t c)
{
    return c == 0x20 || c == 0x09 || c == 0x0d || c == 0x2c || c == 0x28 || c == 0x29 || c == 0x23;
}

/* 0x50d04, 152 bytes. Copy the next token of the command line (0x1a5986) to 0x1a5968. */
void next_token_50d04(void)
{
    int16_t position = (int16_t)EF_U16(0x00127ce2u);
    int16_t length = 0;
    int32_t at;

    while (is_separator_a(EF_U8(0x001a5986u + (uint32_t)(int32_t)position)))
        position++;
    for (;;) {
        EF_U8(0x001a5968u + (uint32_t)(int32_t)length) = EF_U8(0x001a5986u + (uint32_t)(int32_t)position);
        length++;
        at = length;
        position++;
        if (is_separator_b(EF_U8(0x001a5967u + (uint32_t)at)) || !(at < 0x1d))
            break;
    }
    EF_U8(0x001a5967u + (uint32_t)(int32_t)length) = 0;
    EF_U16(0x00127ce2u) = (uint16_t)position;
}

/* 0x544f8, 170 bytes. Copy six owner words into each 0x1b-byte entry of the table at 0x1a5c2e. */
void refresh_owner_entries_544f8(void)
{
    uint8_t i;

    if (EF_U8(0x0012583au) == 0)
        return;
    for (i = 0; i < EF_U8(0x0012583au); i++) {
        ef_addr32 entry = 0x001a5c2eu + (uint32_t)i * 0x1bu;
        ef_addr32 unit = guest_fn_7489c((int16_t)EF_U16(entry + 2u));

        if (unit != 0) {
            ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);

            EF_U32(entry + 5u) = EF_U32(owner + 0x61u);
            EF_U32(entry + 9u) = EF_U32(owner + 0x65u);
            EF_U32(entry + 0xdu) = EF_U32(owner + 0x69u);
            EF_U32(entry + 0x11u) = EF_U32(owner + 0x6du);
            EF_U32(entry + 0x15u) = EF_U32(owner + 0x71u);
            EF_U32(entry + 0x19u) = EF_U32(owner + 0x75u);
        }
    }
}

/* 0x54804, 159 bytes. Apply a part-update message to the object. */
void apply_part_message_54804(ef_addr32 message)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int16_t)EF_U16(message + 1u));
    ef_addr32 part;
    uint16_t flags;

    if (object == 0)
        return;
    part = EF_U32(object + 0x48u) + (uint32_t)((int32_t)(int16_t)EF_U16(message + 3u) * 0x44);
    EF_U16(part + 0xcu) = EF_U16(message + 5u);
    EF_U16(part + 0xeu) = EF_U16(message + 7u);
    EF_U8(part + 0x16u) &= 0xfbu;
    flags = EF_U16(part + 0x16u);
    EF_U16(part + 0x16u) = (uint16_t)(flags | ((EF_U8(message + 9u) & 1u) << 2));
    EF_U16(part + 0x42u) = EF_U16(message + 0xau);
}

/* 0x54f4c, 168 bytes. Apply a unit-state message when it concerns the local player or is open. */
void apply_unit_message_54f4c(ef_addr32 message)
{
    ef_addr32 unit = guest_fn_7489c((int16_t)EF_U16(message + 1u));
    int16_t id;
    int16_t owner;

    if (unit == 0 || EF_U8(0x001f9012u) != 0)
        return;
    id = (int16_t)EF_U16(unit + 0x16u);
    owner = (int16_t)EF_U16(unit + 0x18u);
    if ((uint16_t)EF_U8(0x001a666bu) == (uint16_t)owner && EF_U8(message + 4u) == 0) {
        send_word_byte_packet_24(id, EF_U8(message + 3u));
        return;
    }
    if ((uint16_t)EF_U8(0x001a666bu) != (uint16_t)owner && owner != 0xff && owner != 0xfe)
        return;
    if (owner != 0xfe)
        guest_fn_548e0(id);
    send_unit_pending_54688(id, EF_U8(message + 3u));
}

/* 0x55aac, 152 bytes. Send the full state of a record (command 0x0e). */
void send_record_state_55aac(ef_addr32 record)
{
    uint8_t bytes[0x1e] = {0};
    uint8_t kind = EF_U8(record + 0x18u);

    bytes[0] = 0x0e;
    bytes[0x18] = kind;
    put32(bytes, 1, EF_U32(record));
    put32(bytes, 5, EF_U32(record + 4u));
    put32(bytes, 9, EF_U32(record + 8u));
    put16(bytes, 0xd, EF_U16(record + 0xcu));
    put16(bytes, 0xf, EF_U16(record + 0xeu));
    put16(bytes, 0x11, EF_U16(record + 0x10u));
    bytes[0x13] = EF_U8(0x001a666bu);
    put16(bytes, 0x14, EF_U16(record + 0x54u));
    if (kind == 0x20 || kind == 0x1f) {
        ef_addr32 linked = EF_U32(record + 0x4bu);

        if (linked != 0)
            put16(bytes, 0x16, EF_U16(linked + 0x16u));
    } else {
        put16(bytes, 0x16, EF_U16(record + 0x56u));
    }
    put32(bytes, 0x19, EF_U32(record + 0x26u));
    bytes[0x1d] = EF_U8(record + 0x58u);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56444, 175 bytes. Send the unit's twelve option bytes and flag bits (command 0x11). */
void send_unit_options_56444(int16_t unit_id, uint16_t word_a, uint16_t word_b)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    uint8_t bytes[0x14] = {0};
    uint8_t flags;
    uint8_t i;

    if (unit == 0)
        return;
    bytes[0] = 0x11;
    put16(bytes, 1, (uint16_t)unit_id);
    put16(bytes, 0x10, word_a);
    for (i = 0; i < 0xc; i++)
        bytes[3 + i] = EF_U8(unit + 0x18eu + i);
    flags = (uint8_t)((bytes[0xf] & 0xfeu) | (EF_U8(unit + 0x19au) & 1u));
    flags = (EF_U8(unit + 0x24u) & 4u) ? (uint8_t)(flags | 2u) : (uint8_t)(flags & 0xfdu);
    flags = (EF_U8(unit + 0x24u) & 2u) ? (uint8_t)(flags | 4u) : (uint8_t)(flags & 0xfbu);
    flags = (EF_U8(unit + 0x24u) & 8u) ? (uint8_t)(flags | 8u) : (uint8_t)(flags & 0xf7u);
    bytes[0xf] = flags;
    put16(bytes, 0x12, word_b);
    emit_command_packet(bytes, sizeof(bytes));
}
