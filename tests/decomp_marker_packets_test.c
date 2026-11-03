/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

uint8_t g_world_grid_marker;
ef_addr32 g_world_grid_base;

static uint8_t mem[0x200000];
static struct { uint8_t bytes[8]; uint32_t size; } seen[16];
static unsigned count;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(mem));
    return mem + address;
}

void emit_command_packet(const uint8_t *bytes, uint32_t size)
{
    assert(count < 16 && size <= 8);
    seen[count].size = size;
    memcpy(seen[count].bytes, bytes, size);
    ++count;
}

static void expect(unsigned slot, const uint8_t *bytes, uint32_t size)
{
    assert(seen[slot].size == size);
    assert(memcmp(seen[slot].bytes, bytes, size) == 0);
}

static unsigned refreshed, applied, notified, queried;
static uint32_t applied_value, query_result;
static uint8_t applied_flag, query_selector;
static int32_t query_high;
void guest_refresh_activity_records(void) { ++refreshed; }
void guest_apply_marker_update(uint32_t value, uint8_t flag)
{
    ++applied;
    applied_value = value;
    applied_flag = flag;
}
uint32_t guest_query_scene_pair(uint8_t selector, int32_t high)
{
    ++queried;
    query_selector = selector;
    query_high = high;
    return query_result;
}
void guest_notify_scene_pair(void) { ++notified; }

static unsigned acquired;
static uint32_t set_object, set_value, lookup_arg;
void guest_runtime_acquire(ef_addr32 object) { ++acquired; set_object = object; }
void guest_runtime_set(ef_addr32 object, uint32_t value) { assert(object == set_object); set_value = value; }
uint16_t guest_load_halfword(ef_addr32 source) { return (uint16_t)(source & 0xffffu); }
uint32_t guest_lookup_value(ef_addr32 source) { lookup_arg = source; return source ^ 0x55u; }
static uint16_t selector_word;
uint16_t guest_read_selector_word(void) { return selector_word; }

int main(void)
{
    const uint8_t p1d[1] = {0x1d}, p1f[1] = {0x1f};
    const uint8_t p26[2] = {0x26, 0x9c}, p1e[2] = {0x1e, 0x9c};
    const uint8_t p24[4] = {0x24, 0x01, 0x80, 0xe5};
    const uint8_t p1b[2] = {0x1b, 0x9c};

    g_world_grid_marker = 0x9c;
    send_command_1d();
    send_marker_packet_26();
    send_marker_packet_1e();
    send_command_1f();
    send_word_byte_packet_24((int16_t)0x8001, 0xe5);
    send_marker_packet_1b();
    expect(0, p1d, 1);
    expect(1, p26, 2);
    expect(2, p1e, 2);
    expect(3, p1f, 1);
    expect(4, p24, 4);
    expect(5, p1b, 2);

    /* commit_marker_selection: copies byte +1, packet 1b, latch. */
    mem[0x1000] = 0xaa;
    mem[0x1001] = 0x5d;
    commit_marker_selection(0x1000);
    assert(mem[0x12583b] == 0x5d && mem[0x1a5d1c] == 1);
    assert(count == 7);
    expect(6, p1b, 2);

    /* apply_marker_to_grid_row: matching marker uses a signed word index. */
    g_world_grid_base = 0x180000;
    mem[0x2000] = 0;
    mem[0x2001] = 0xff;
    mem[0x2002] = 0xff; /* index -1 */
    mem[0x2003] = 0x9c;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 - 0xca] == 0x9c);
    mem[0x2001] = 3;
    mem[0x2002] = 0;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 + 3 * 0xca] == 0x9c);
    /* non-matching marker leaves the row untouched */
    mem[0x2003] = 0x9d;
    mem[0x2001] = 5;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 + 5 * 0xca] == 0);

    /* second batch */
    {
        const uint8_t a4f[5] = {0x4f, 0x34, 0x12, 0xff, 0x80};
        const uint8_t a42[6] = {0x42, 0x01, 0x00, 0xfe, 0xff, 0x7a};
        const uint8_t a2a[6] = {0x2a, 0x78, 0x56, 0x34, 0x92, 0x9c};
        const uint8_t a30[3] = {0x30, 0x9c, 0x00};
        unsigned base = count;
        send_two_word_packet_4f(0x1234, 0x80ff);
        send_two_word_byte_packet_42(1, 0xfffe, 0x7a);
        send_dword_marker_packet_2a(0x92345678u);
        send_marker_word_packet_30();
        expect(base, a4f, 5);
        expect(base + 1, a42, 6);
        expect(base + 2, a2a, 6);
        expect(base + 3, a30, 3);
    }
    mem[0x1a6630] = mem[0x1a6631] = mem[0x127dae] = mem[0x127daf] = 0xff;
    clear_scene_word_latches();
    assert(!mem[0x1a6630] && !mem[0x1a6631] && !mem[0x127dae] && !mem[0x127daf]);

    mem[0x3000] = 9;
    mem[0x3001] = 2;
    mem[0x1a5c32 + 46] = 0xff;
    clear_activity_record_flag(0x3000);
    assert(mem[0x1a5c32 + 46] == 0 && refreshed == 1);
    mem[0x3001] = 0xff; /* index is unsigned: 255 * 23 */
    mem[0x1a5c32 + 255 * 23] = 1;
    clear_activity_record_flag(0x3000);
    assert(mem[0x1a5c32 + 255 * 23] == 0 && refreshed == 2);

    /* forward_marker_matched_record: low nibble compares with the marker. */
    g_world_grid_marker = 0x05;
    mem[0x3101] = 0xf5; mem[0x3102] = 0x12; mem[0x3103] = 0x34; mem[0x3104] = 0x56;
    mem[0x3105] = 0xc3;
    forward_marker_matched_record(0x3100);
    assert(applied == 1 && applied_value == 0x563412f5u && applied_flag == 0xc3);
    g_world_grid_marker = 0x06;
    forward_marker_matched_record(0x3100);
    assert(applied == 1);

    /* notify_if_scene_pair: byte +0xd, signed high word of dword +0xc. */
    mem[0x320c] = 0; mem[0x320d] = 0x42; mem[0x320e] = 0xfe; mem[0x320f] = 0xff;
    query_result = 0;
    notify_if_scene_pair(0x3200);
    assert(queried == 1 && query_selector == 0x42 && query_high == -2 && notified == 0);
    query_result = 0x10000;
    notify_if_scene_pair(0x3200);
    assert(notified == 1);

    /* third batch */
    {
        unsigned base = count;
        const uint8_t b3e[5] = {0x3e, 0x11, 0x34, 0x12, 0x77};
        const uint8_t b3b[6] = {0x3b, 0x01, 0x80, 0xff, 0x7f, 0x55};
        const uint8_t b3d[4] = {0x3d, 0xfe, 0xff, 0x09};
        const uint8_t b40[5] = {0x40, 0x34, 0x12, 0x78, 0x56};
        const uint8_t b2e[3] = {0x2e, 0x02, 0x81};
        send_byte_word_byte_packet_3e(0x11, 0x1234, 0x77);
        send_two_word_byte_packet_3b(0x8001, 0x7fff, 0x55);
        send_word_byte_packet_3d(0xfffe, 9);
        send_two_word_packet_40(0x1234, 0x5678);
        send_word_packet_2e(0x8102);
        expect(base, b3e, 5);
        expect(base + 1, b3b, 6);
        expect(base + 2, b3d, 4);
        expect(base + 3, b40, 5);
        expect(base + 4, b2e, 3);
    }
    mem[0x3400] = 0; mem[0x3401] = 3; mem[0x3402] = 0;
    bump_counter_19b960(0x3400);
    bump_counter_19b96c(0x3400);
    bump_counter_19b966(0x3400);
    assert(mem[0x19b963] == 1 && mem[0x19b96f] == 1 && mem[0x19b969] == 1);
    mem[0x3401] = 4;
    bump_counter_19b960(0x3400);
    assert(mem[0x19b964] == 0);
    mem[0x3401] = 0xff; mem[0x3402] = 0xff; /* -1 passes the < 4 test */
    bump_counter_19b966(0x3400);
    assert(mem[0x19b965] == 1);
    set_mode_limits_4_5();
    assert(mem[0x1a6ce2] == 4 && mem[0x1a6ce3] == 0 && mem[0x1a6ce8] == 5);
    set_mode_limits_8_10();
    assert(mem[0x1a6ce2] == 8 && mem[0x1a6ce8] == 10);

    /* fourth batch */
    mem[0x127d3e] = 0x34; mem[0x127d3f] = 0x12;
    apply_runtime_word_forward();
    assert(acquired == 1 && set_object == 0x1a7c30 && set_value == 0x1234);
    apply_runtime_word_negated();
    assert(acquired == 2 && set_value == 0xedcc);
    mem[0x127d3e] = 0; mem[0x127d3f] = 0;
    apply_runtime_word_negated();
    assert(set_value == 0);
    load_halfword_pair();
    assert(mem[0x1f6f38] == 0x68 && mem[0x1f6f39] == 0x62 && mem[0x1f6f42] == 0x70);
    load_word_0x1282dc();
    assert(mem[0x1282dc] == (uint8_t)(0x186278 ^ 0x55) && mem[0x1282dd] == 0x62);
    mem[0x128564 + 2 * 0xe5] = 0x10; mem[0x128564 + 2 * 0xe5 + 1] = 0x20;
    assert(lookup_record_value(2) == (0x2010u ^ 0x55u) && lookup_arg == 0x2010);
    mem[0x128568] = 4; assert(record_mode_is_four(0) == 1);
    for (unsigned m = 0; m < 256; ++m) {
        mem[0x128568 + 0xe5] = (uint8_t)m;
        assert(record_mode_is_four(1) == (m == 4));
    }
    selector_word = 0x230 * 3 + 5;
    mem[0x12a760 + 12] = 0xaa; mem[0x12a760 + 13] = 0xbb;
    assert(lookup_table_by_quotient() == 0xbbaau);
    selector_word = 0xffff;
    (void)0;
    mem[0x12a760 + (0xffff / 0x230) * 4] = 0x42;
    assert(lookup_table_by_quotient() == 0x42);
    reset_pair_counter();
    assert(mem[0x1f891e] == 0xff && mem[0x1f891f] == 0xff && mem[0x1f891c] == 0);
    return 0;
}
