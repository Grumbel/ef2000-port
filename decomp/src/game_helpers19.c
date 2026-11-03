/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network message handlers and packet builders, 0x566b4..0x573d0. */

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

/* 0x566b4, 136 bytes. Load the parameter block from a message and apply it for the local player. */
void apply_params_message_566b4(ef_addr32 message)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int16_t)EF_U16(message + 1u));

    if (object == 0)
        return;
    EF_U16(0x00197c28u) = EF_U16(message + 5u);
    EF_U16(0x00197c2au) = EF_U16(message + 7u);
    EF_U16(0x00197c2cu) = EF_U16(message + 9u);
    EF_U16(0x00197c24u) = EF_U16(message + 0xbu);
    EF_U16(0x00197c26u) = EF_U16(message + 0xdu);
    EF_U8(0x00121ec6u) = EF_U8(message + 0xfu);
    EF_U8(0x00197c2eu) = EF_U8(message + 0x10u);
    if ((uint16_t)EF_U8(0x001a666bu) == EF_U16(object + 0x18u))
        guest_fn_900c0((int16_t)EF_U16(message + 1u), EF_U8(message + 3u), (int16_t)EF_U16(message + 0x10u));
}

/* 0x572b4, 148 bytes. Forward a command message as packet 0x48/0x49/0x47 plus a closing 0x4a. */
void forward_command_message_572b4(ef_addr32 message)
{
    int16_t id = (int16_t)EF_U16(message + 1u);
    int16_t extra = (int16_t)EF_U16(message + 3u);
    ef_addr32 unit = guest_fn_7489c((int32_t)id);
    uint8_t kind;

    if (unit == 0)
        return;
    if ((uint16_t)EF_U8(0x001a666bu) != EF_U16(unit + 0x18u) && (int16_t)EF_U16(unit + 0x18u) != 0xff)
        return;
    kind = EF_U8(message + 5u);
    if (kind == 0)
        send_packet_15(id, extra, 0x48, 0, 0, 0);
    else if (kind == 1)
        send_packet_15(id, extra, 0x49, 0, 0, 0);
    else if (kind == 2)
        send_packet_15(id, extra, 0x47, 0, 0, 0);
    send_packet_15(id, extra, 0x4a, 0, 0, 0);
}

/* 0x573d0, 176 bytes. Send entry idx of the 48-byte table at 0x211af0 (command 0x3f). */
void send_table_entry_573d0(int16_t index, uint8_t value)
{
    ef_addr32 entry = 0x00211af0u + (uint32_t)((int32_t)index * 48);
    uint8_t bytes[0x1c] = {0};

    bytes[0] = 0x3f;
    put32(bytes, 1, EF_U32(entry));
    put32(bytes, 5, EF_U32(entry + 4u));
    put32(bytes, 9, EF_U32(entry + 8u));
    put16(bytes, 0xd, EF_U16(entry + 0xcu));
    put16(bytes, 0xf, EF_U16(entry + 0xeu));
    put16(bytes, 0x11, EF_U16(entry + 0x10u));
    bytes[0x13] = (uint8_t)(EF_U16(entry + 0x24u) & 1u);
    bytes[0x14] = (uint8_t)((EF_U16(entry + 0x24u) >> 1) & 1u);
    put16(bytes, 0x15, (uint16_t)index);
    put16(bytes, 0x19, EF_U8(0x001a666bu));
    put16(bytes, 0x17, EF_U16(entry + 0x2eu));
    bytes[0x1b] = value;
    emit_command_packet(bytes, sizeof(bytes));
}
