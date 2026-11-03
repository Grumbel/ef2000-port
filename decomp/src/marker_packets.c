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

static void put_word(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

/* 0x571f0, 37 bytes. Five-byte packet 0x3e: AL, DX word, BL. */
void send_byte_word_byte_packet_3e(uint8_t first, uint16_t second, uint8_t third)
{
    uint8_t bytes[5];
    bytes[0] = 0x3e;
    bytes[1] = first;
    put_word(bytes + 2, second);
    bytes[4] = third;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x5728c, 38 bytes. Six-byte packet 0x3b: AX, DX, BL; BH is clobbered. */
void send_two_word_byte_packet_3b(uint16_t first, uint16_t second, uint8_t flag)
{
    uint8_t bytes[6];
    bytes[0] = 0x3b;
    put_word(bytes + 1, first);
    put_word(bytes + 3, second);
    bytes[5] = flag;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x57348, 33 bytes. Four-byte packet 0x3d: AX word, DL. */
void send_word_byte_packet_3d(uint16_t value, uint8_t flag)
{
    uint8_t bytes[4];
    bytes[0] = 0x3d;
    put_word(bytes + 1, value);
    bytes[3] = flag;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x575fc, 36 bytes. Five-byte packet 0x40: AX then DX words. */
void send_two_word_packet_40(uint16_t first, uint16_t second)
{
    uint8_t bytes[5];
    bytes[0] = 0x40;
    put_word(bytes + 1, first);
    put_word(bytes + 3, second);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x57650, 31 bytes. Three-byte packet 0x2e: AX word. */
void send_word_packet_2e(uint16_t value)
{
    uint8_t bytes[3];
    bytes[0] = 0x2e;
    put_word(bytes + 1, value);
    emit_command_packet(bytes, sizeof(bytes));
}

static void bump_counter(ef_addr32 record, ef_addr32 base)
{
    int32_t index = (int16_t)(*(const uint8_t *)ef_guest_memory_ptr(record + 1u) |
                              (uint16_t)(*(const uint8_t *)ef_guest_memory_ptr(record + 2u) << 8));
    /* ORIGINAL BUG: only the upper bound is checked; negative indexes
     * write below the table. */
    if (index < 4)
        ++*(uint8_t *)ef_guest_memory_ptr(base + (ef_addr32)index);
}

/* 0x576a4, 16 bytes. Increment byte 0x19b960 + signed word record+1 if < 4. */
void bump_counter_19b960(ef_addr32 record)
{
    bump_counter(record, 0x0019b960u);
}

/* 0x576d4, 16 bytes. Same shape on base 0x19b96c. */
void bump_counter_19b96c(ef_addr32 record)
{
    bump_counter(record, 0x0019b96cu);
}

/* 0x57704, 16 bytes. Same shape on base 0x19b966. */
void bump_counter_19b966(ef_addr32 record)
{
    bump_counter(record, 0x0019b966u);
}

/* 0x58d04, 29 bytes. Set the two mode limit words to 4 and 5. */
void set_mode_limits_4_5(void)
{
    *(uint16_t *)ef_guest_memory_ptr(0x001a6ce2u) = 4;
    *(uint16_t *)ef_guest_memory_ptr(0x001a6ce8u) = 5;
}

/* 0x58d44, 29 bytes. Set the two mode limit words to 8 and 10. */
void set_mode_limits_8_10(void)
{
    *(uint16_t *)ef_guest_memory_ptr(0x001a6ce2u) = 8;
    *(uint16_t *)ef_guest_memory_ptr(0x001a6ce8u) = 10;
}

static uint16_t read_word(ef_addr32 address)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(address);
    return (uint16_t)(p[0] | (uint16_t)(p[1] << 8));
}

static uint32_t read_dword(ef_addr32 address)
{
    return (uint32_t)read_word(address) | (uint32_t)read_word(address + 2u) << 16;
}

static void write_word(ef_addr32 address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

/* 0x6454c, 32 bytes. Acquire object 0x1a7c30 then set it to word 0x127d3e.
 * Its only direct caller is the function entered at 0x624f4. */
void apply_runtime_word_forward(void)
{
    guest_runtime_acquire(0x001a7c30u);
    guest_runtime_set(0x001a7c30u, read_word(0x00127d3eu));
}

/* 0x64524, 38 bytes. As 0x6454c, but with the 16-bit negation of the word. */
void apply_runtime_word_negated(void)
{
    guest_runtime_acquire(0x001a7c30u);
    guest_runtime_set(0x001a7c30u, (uint16_t)(0u - read_word(0x00127d3eu)));
}

/* 0x68bf4, 33 bytes. Load two halfwords from guest routine 0xde864. */
void load_halfword_pair(void)
{
    write_word(0x001f6f38u, guest_load_halfword(0x00186268u));
    write_word(0x001f6f42u, guest_load_halfword(0x00186270u));
}

/* 0x69080, 17 bytes. Store the low word returned by 0x9ddfc(0x186278). */
void load_word_0x1282dc(void)
{
    write_word(0x001282dcu, (uint16_t)guest_lookup_value(0x00186278u));
}

/* 0x6a0f8, 22 bytes. Tail-call 0x9ddfc with the dword at +0 of the
 * 0xe5-byte record selected by AL, rooted at 0x128564. */
uint32_t lookup_record_value(uint8_t index)
{
    return guest_lookup_value(read_dword(0x00128564u + (uint32_t)index * 0xe5u));
}

/* 0x6b420, 35 bytes. True only for mode code 4 (codes 2 and 3 fall
 * through the same false path as the original). */
uint8_t record_mode_is_four(uint8_t index)
{
    uint8_t mode = *(const uint8_t *)ef_guest_memory_ptr(0x00128568u + (uint32_t)index * 0xe5u);
    if (mode < 2)
        return 0;
    if (mode <= 3)
        return 0;
    return mode == 4;
}

/* 0x77afc, 36 bytes. AX from 0xa45b3, unsigned divided by 0x230, selects
 * a dword in the table at 0x12a760. */
uint32_t lookup_table_by_quotient(void)
{
    uint32_t value = guest_read_selector_word();
    return read_dword(0x0012a760u + (value / 0x230u) * 4u);
}

/* 0x7772c, 26 bytes. Set word 0x1f891e to 0xffff and word 0x1f891c to 0. */
void reset_pair_counter(void)
{
    write_word(0x001f891eu, 0xffffu);
    write_word(0x001f891cu, 0);
}
