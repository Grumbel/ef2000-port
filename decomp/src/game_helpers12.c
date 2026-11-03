/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Command-packet builders emitted through guest helper 0xa3f74.
 * The original leaves unwritten stack bytes in the packet uninitialised;
 * the host buffers below zero them. */

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

/* Shared tail of the 0x197c24 parameter block, bytes 0x5..0xf (or 0x4..0xe). */
static void put_params(uint8_t *bytes, uint32_t offset)
{
    put16(bytes, offset, EF_U16(0x00197c28u));
    put16(bytes, offset + 2, EF_U16(0x00197c2au));
    put16(bytes, offset + 4, EF_U16(0x00197c2cu));
    put16(bytes, offset + 6, EF_U16(0x00197c24u));
    put16(bytes, offset + 8, EF_U16(0x00197c26u));
    bytes[offset + 10] = EF_U8(0x00121ec6u);
}

/* 0x54688, 53 bytes. Mark the unit as pending (0xfe) and send command 0x0d. */
void send_unit_pending_54688(int16_t unit_id, uint8_t value)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    uint8_t bytes[4] = {0};

    if (unit == 0)
        return;
    EF_U16(unit + 0x18u) = 0xfe;
    bytes[0] = 0x0d;
    put16(bytes, 1, (uint16_t)unit_id);
    bytes[3] = value;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x546e8, 56 bytes. Mark the owner record as pending (0xfe) and send command 0x17. */
void send_owner_pending_546e8(int16_t owner, uint8_t value)
{
    uint8_t bytes[4] = {0};

    EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca)) = 0xfe;
    bytes[0] = 0x17;
    put16(bytes, 1, (uint16_t)owner);
    bytes[3] = value;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54720, 100 bytes. Send the position block of object id (command 0x21). */
void send_object_block_54720(int16_t object_id)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int32_t)object_id);
    uint8_t bytes[0x19] = {0};

    if (object == 0)
        return;
    bytes[0] = 0x21;
    put16(bytes, 1, EF_U16(object + 0x16u));
    put32(bytes, 3, EF_U32(object));
    put32(bytes, 7, EF_U32(object + 4u));
    put32(bytes, 0xb, EF_U32(object + 8u));
    put16(bytes, 0xf, EF_U16(object + 0xcu));
    put16(bytes, 0x11, EF_U16(object + 0xeu));
    put16(bytes, 0x13, EF_U16(object + 0x10u));
    put32(bytes, 0x15, EF_U32(object + 0x12u));
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54784, 125 bytes. Send one 0x44-byte sub-record of the object (command 0x22). */
void send_object_part_54784(int16_t object_id, int16_t part)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int32_t)object_id);
    uint8_t bytes[0xc] = {0};
    uint32_t offset;

    if (object == 0)
        return;
    offset = (uint32_t)((int32_t)part * 0x44);
    bytes[0] = 0x22;
    put16(bytes, 1, (uint16_t)object_id);
    put16(bytes, 3, (uint16_t)part);
    put16(bytes, 5, EF_U16(EF_U32(object + 0x48u) + offset + 0xcu));
    put16(bytes, 7, EF_U16(EF_U32(object + 0x48u) + offset + 0xeu));
    bytes[9] = (uint8_t)((EF_U16(EF_U32(object + 0x48u) + offset + 0x16u) >> 2) & 1u);
    put16(bytes, 0xa, EF_U16(EF_U32(object + 0x48u) + offset + 0x42u));
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55dac, 62 bytes. Send a position vector and a word (command 0x0f). */
void send_vector_55dac(ef_addr32 vector, uint16_t value)
{
    uint8_t bytes[0x11] = {0};

    bytes[0] = 0x0f;
    put32(bytes, 1, EF_U32(vector));
    put32(bytes, 5, EF_U32(vector + 4u));
    put32(bytes, 9, EF_U32(vector + 8u));
    bytes[0xd] = EF_U8(0x001a666bu);
    put16(bytes, 0xf, value);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55dec, 45 bytes. Send the word at +0x54 of the record (command 0x10). */
void send_record_word_55dec(ef_addr32 record)
{
    uint8_t bytes[0x10] = {0};

    bytes[0] = 0x10;
    bytes[0xd] = EF_U8(0x001a666bu);
    put16(bytes, 0xe, EF_U16(record + 0x54u));
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55e1c, 120 bytes. Send the parameter block for an object (command 0x28). */
void send_object_params_55e1c(int16_t object_id, uint8_t kind, uint16_t extra)
{
    uint8_t bytes[0x12] = {0};

    if (guest_lookup_object_8ffe8((int32_t)object_id) == 0)
        return;
    bytes[0] = 0x28;
    put16(bytes, 1, (uint16_t)object_id);
    put16(bytes, 3, kind);
    put16(bytes, 5, EF_U16(0x00197c28u));
    put16(bytes, 7, EF_U16(0x00197c2au));
    put16(bytes, 9, EF_U16(0x00197c2cu));
    put16(bytes, 0xb, EF_U16(0x00197c24u));
    put16(bytes, 0xd, EF_U16(0x00197c26u));
    bytes[0xf] = EF_U8(0x00121ec6u);
    put16(bytes, 0x10, extra);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55e94, 119 bytes. Send the parameter block for a unit (command 0x23). */
void send_unit_params_55e94(int16_t unit_id, uint8_t kind)
{
    uint8_t bytes[0x10] = {0};

    if (guest_fn_7489c((int32_t)unit_id) == 0)
        return;
    bytes[0] = 0x23;
    put16(bytes, 1, (uint16_t)unit_id);
    bytes[3] = kind;
    bytes[4] = EF_U8(0x001a666bu);
    put_params(bytes, 5);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55f0c, 69 bytes. Send an object's parameters with four caller words (command 0x27). */
void send_object_words_55f0c(int16_t object_id, uint16_t b, uint16_t c, uint16_t d)
{
    uint8_t bytes[0xe] = {0};

    if (guest_lookup_object_8ffe8((int32_t)object_id) == 0)
        return;
    bytes[0] = 0x27;
    put16(bytes, 1, (uint16_t)object_id);
    put16(bytes, 6, b);
    put16(bytes, 8, c);
    put16(bytes, 0xa, EF_U16(0x00197c28u));
    put16(bytes, 0xc, d);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56f6c, 58 bytes. Send the three words at 0x12583c (command 0x34). */
void send_triple_56f6c(void)
{
    uint8_t bytes[7] = {0};

    bytes[0] = 0x34;
    put16(bytes, 1, EF_U16(0x0012583cu));
    put16(bytes, 3, EF_U16(0x0012583eu));
    put16(bytes, 5, EF_U16(0x00125840u));
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x570c8, 51 bytes. */
void send_player_57_570c8(void)
{
    uint8_t bytes[2];

    guest_fn_78aa8(0x0012f3d3u);
    bytes[0] = 0x37;
    bytes[1] = EF_U8(0x001a666bu);
    emit_command_packet(bytes, sizeof(bytes));
    guest_fn_a3f30();
}
