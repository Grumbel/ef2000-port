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

/* 0x55760, 36 bytes. Five-byte packet 0x4f: AX then DX, little-endian. */
void send_two_word_packet_4f(uint16_t first, uint16_t second)
{
    uint8_t bytes[5];
    bytes[0] = 0x4f;
    bytes[1] = (uint8_t)first;
    bytes[2] = (uint8_t)(first >> 8);
    bytes[3] = (uint8_t)second;
    bytes[4] = (uint8_t)(second >> 8);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55bd0, 38 bytes. Six-byte packet 0x42: AX, DX, then BL. The original
 * loads the opcode through BH and so leaves EBX bits 8..15 changed. */
void send_two_word_byte_packet_42(uint16_t first, uint16_t second, uint8_t flag)
{
    uint8_t bytes[6];
    bytes[0] = 0x42;
    bytes[1] = (uint8_t)first;
    bytes[2] = (uint8_t)(first >> 8);
    bytes[3] = (uint8_t)second;
    bytes[4] = (uint8_t)(second >> 8);
    bytes[5] = flag;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56004, 19 bytes. Clear two word latches. */
void clear_scene_word_latches(void)
{
    *(uint16_t *)ef_guest_memory_ptr(0x001a6630u) = 0;
    *(uint16_t *)ef_guest_memory_ptr(0x00127daeu) = 0;
}

/* 0x561bc, 39 bytes. Six-byte packet 0x2a: EAX little-endian, grid marker. */
void send_dword_marker_packet_2a(uint32_t value)
{
    uint8_t bytes[6];
    bytes[0] = 0x2a;
    bytes[1] = (uint8_t)value;
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)(value >> 16);
    bytes[4] = (uint8_t)(value >> 24);
    bytes[5] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56d40, 37 bytes. Three-byte packet 0x30 carrying the marker as a
 * zero-extended word. */
void send_marker_word_packet_30(void)
{
    uint8_t bytes[3];
    bytes[0] = 0x30;
    bytes[1] = g_world_grid_marker;
    bytes[2] = 0;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55860, 35 bytes. Clear byte 0 of the 23-byte activity record selected by
 * record byte +1, then refresh through the guest routine 0x544c0. */
void clear_activity_record_flag(ef_addr32 record)
{
    uint32_t index = *(const uint8_t *)ef_guest_memory_ptr(record + 1u);
    *(uint8_t *)ef_guest_memory_ptr(0x001a5c32u + index * 23u) = 0;
    guest_refresh_activity_records();
}

/* 0x561e4, 36 bytes. If the low nibble of the dword at +1 equals the grid
 * marker, forward that dword and the byte at +5 to guest routine 0x562d8. */
void forward_marker_matched_record(ef_addr32 record)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(record);
    uint32_t value = (uint32_t)p[1] | (uint32_t)p[2] << 8 |
                     (uint32_t)p[3] << 16 | (uint32_t)p[4] << 24;
    if ((value & 0xfu) == (uint32_t)g_world_grid_marker)
        guest_apply_marker_update(value, p[5]);
}

/* 0x56954, 31 bytes. Query with the byte at +0xd and signed high word of
 * the dword at +0xc; any nonzero EAX result triggers 0x878c8. */
void notify_if_scene_pair(ef_addr32 record)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(record + 0xcu);
    int32_t dword = (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 |
                              (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
    if (guest_query_scene_pair(p[1], dword >> 16) != 0)
        guest_notify_scene_pair();
}
