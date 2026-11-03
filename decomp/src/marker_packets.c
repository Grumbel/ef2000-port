/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x545e4, 26 bytes. One-byte packet 0x1d; the original builds it in AH. */
void send_command_1d(void)
{
    uint8_t bytes[1];
    bytes[0] = 0x1d;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54600, 34 bytes. Two-byte packet 0x26 plus the grid marker. */
void send_marker_packet_26(void)
{
    uint8_t bytes[2];
    bytes[0] = 0x26;
    bytes[1] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54624, 34 bytes. Two-byte packet 0x1e plus the grid marker. */
void send_marker_packet_1e(void)
{
    uint8_t bytes[2];
    bytes[0] = 0x1e;
    bytes[1] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54648, 26 bytes. One-byte packet 0x1f. */
void send_command_1f(void)
{
    uint8_t bytes[1];
    bytes[0] = 0x1f;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54664, 33 bytes. Four-byte packet 0x24, AX little-endian, then DL. */
void send_word_byte_packet_24(int16_t value, uint8_t flag)
{
    uint16_t word = (uint16_t)value;
    uint8_t bytes[4];
    bytes[0] = 0x24;
    bytes[1] = (uint8_t)word;
    bytes[2] = (uint8_t)(word >> 8);
    bytes[3] = flag;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54e5c, 34 bytes. Two-byte packet 0x1b plus the grid marker. */
void send_marker_packet_1b(void)
{
    uint8_t bytes[2];
    bytes[0] = 0x1b;
    bytes[1] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x54e9c, 21 bytes. Copy record byte +1 to 0x12583b, send packet 0x1b,
 * then set the pending latch at 0x1a5d1c. */
void commit_marker_selection(ef_addr32 record)
{
    *(uint8_t *)ef_guest_memory_ptr(0x0012583bu) =
        *(const uint8_t *)ef_guest_memory_ptr(record + 1u);
    send_marker_packet_1b();
    *(uint8_t *)ef_guest_memory_ptr(0x001a5d1cu) = 1;
}

/* 0x550b0, 39 bytes. If record byte +3 equals the grid marker, write the
 * marker into byte 0 of the grid row selected by signed word +1. */
void apply_marker_to_grid_row(ef_addr32 record)
{
    const uint8_t *source = (const uint8_t *)ef_guest_memory_ptr(record);
    if (g_world_grid_marker == source[3]) {
        int32_t index = (int16_t)(source[1] | (uint16_t)(source[2] << 8));
        ef_addr32 row = g_world_grid_base + (ef_addr32)(index * 0xca);
        *(uint8_t *)ef_guest_memory_ptr(row) = g_world_grid_marker;
    }
}
