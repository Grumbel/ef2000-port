/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* All eight wrappers pass EAX=pointer to their temporary stack bytes and
 * EDX=byte length into the original game helper at 0xa3f74. The host
 * dependency records bytes synchronously before the stack buffer expires. */
static void send_three_bytes(uint8_t opcode, int16_t index)
{
    uint16_t word = (uint16_t)index;
    uint8_t bytes[3];
    bytes[0] = opcode;
    bytes[1] = (uint8_t)word;
    bytes[2] = (uint8_t)(word >> 8);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56f0c, 0x21 bytes, four caller functions. */
void send_object_state_packet(int16_t index, uint8_t value)
{
    uint16_t word = (uint16_t)index;
    uint8_t bytes[4];
    bytes[0] = 0x32;
    bytes[1] = (uint8_t)word;
    bytes[2] = (uint8_t)(word >> 8);
    bytes[3] = value;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x57140, 0x1f bytes, two caller functions. */
void send_object_code_39(int16_t index)
{
    send_three_bytes(0x39, index);
}

/* 0x57198, 0x1f bytes, two caller functions. */
void send_object_code_3a(int16_t index)
{
    send_three_bytes(0x3a, index);
}

/* 0x57684, 0x1f bytes, two caller functions. */
void send_object_code_4a(int16_t index)
{
    send_three_bytes(0x4a, index);
}

/* 0x576b4, 0x1f bytes, two caller functions. */
void send_object_code_4b(int16_t index)
{
    send_three_bytes(0x4b, index);
}

/* 0x576e4, 0x1f bytes, two caller functions. */
void send_object_code_4c(int16_t index)
{
    send_three_bytes(0x4c, index);
}

/* 0x545b8, 0x2a bytes, two caller functions. Includes the runtime grid
 * marker at offset three, and the original low DL byte at offset four. */
void send_grid_marker_packet(int16_t index, uint8_t value)
{
    uint16_t word = (uint16_t)index;
    uint8_t bytes[5];
    bytes[0] = 0x0c;
    bytes[1] = (uint8_t)word;
    bytes[2] = (uint8_t)(word >> 8);
    bytes[3] = g_world_grid_marker;
    bytes[4] = value;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x546c0, 0x28 bytes, three caller functions. */
void send_grid_marker_short_packet(int16_t index)
{
    uint16_t word = (uint16_t)index;
    uint8_t bytes[4];
    bytes[0] = 0x16;
    bytes[1] = (uint8_t)word;
    bytes[2] = (uint8_t)(word >> 8);
    bytes[3] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}
