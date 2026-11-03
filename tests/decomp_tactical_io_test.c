/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"
#include <assert.h>
#include <string.h>

static uint8_t guest[0x220000];

uint8_t g_reference_copy_mode;
uint16_t g_reference_source_word;
uint8_t g_reference_mirror[4];
uint8_t g_world_grid_marker;
ef_tactical_ring_entry g_tactical_ring[20];
int16_t g_tactical_ring_cursor;
ef_addr32 g_text_scan_cursor;
uint8_t g_text_line_buffer[256];
uint16_t g_text_line_column;
uint8_t g_text_line_count;
uint16_t g_active_unit_record_count;
ef_unit_scan_record g_active_unit_records[24];
uint16_t g_round_robin_cursor;
int16_t g_round_robin_count;
uint16_t g_round_robin_ids[32];
uint8_t g_reset_flag_a, g_reset_flag_b, g_reset_flag_c, g_reset_flag_d;
uint32_t g_reset_accumulator_a[2], g_reset_accumulator_b[2], g_reset_accumulator_c[2];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

static void test_reference_mirror(void)
{
    g_reference_copy_mode = 2;
    memset(g_reference_mirror, 0x55, sizeof(g_reference_mirror));
    assert(mirror_reference_state() == 2);
    assert(g_reference_mirror[0] == 0x55);
    g_reference_copy_mode = 1;
    g_reference_source_word = 0x1234;
    g_world_grid_marker = 2;
    guest[0x19bbf2 + 2] = 0xa2;
    guest[0x19bbfa + 2] = 0xb4;
    assert(mirror_reference_state() == 0xb4);
    assert(g_reference_mirror[0] == 0x34 && g_reference_mirror[1] == 0x12);
    assert(g_reference_mirror[2] == 0xa2 && g_reference_mirror[3] == 0xb4);
    g_world_grid_marker = 0;
    guest[0x19bbf2] = 0x19;
    guest[0x19bbfa] = 0x23;
    assert(mirror_reference_state() == 0x23);
}

static void test_normalize(void)
{
    static const char example[] = "alpha\r\n; ignored\r\nbeta\r\n\r\n\nend\n";
    memcpy(guest + 0x200, example, sizeof(example));
    normalize_script_line_endings(0x200);
    assert(strcmp((char *)guest + 0x200, "alpha\nbeta\nend\n") == 0);
    memcpy(guest + 0x300, ";comment\n\n\nhello\n", 18);
    normalize_script_line_endings(0x300);
    assert(strcmp((char *)guest + 0x300, "\nhello\n") == 0);
}

static void test_ring(void)
{
    g_tactical_ring_cursor = 19;
    memset(g_tactical_ring, 0xfe, sizeof(g_tactical_ring));
    append_tactical_ring_event(-123, 456, 0xc3);
    assert(g_tactical_ring_cursor == 0);
    assert(g_tactical_ring[19].enabled == 1);
    assert(g_tactical_ring[19].x == -123 && g_tactical_ring[19].y == 456);
    assert(g_tactical_ring[19].kind == 0xc3 && g_tactical_ring[19].reserved == 0);
    append_tactical_ring_event(7, -9, 12);
    assert(g_tactical_ring_cursor == 1);
    assert(g_tactical_ring[0].x == 7 && g_tactical_ring[0].y == -9);
}

static void test_lines(void)
{
    const char *source = "  \tfirst line\n  second\n";
    memcpy(guest + 0x400, source, strlen(source) + 1);
    g_text_scan_cursor = 0x400;
    g_text_line_column = 0xeeee;
    g_text_line_count = 255;
    copy_next_text_line();
    assert(strcmp((char *)g_text_line_buffer, "first line\n") == 0);
    assert(g_text_line_column == 0 && g_text_line_count == 0);
    copy_next_text_line();
    assert(strcmp((char *)g_text_line_buffer, "second\n") == 0);
    assert(g_text_scan_cursor == 0x400 + strlen(source));
    assert(g_text_line_count == 1);
}

static void test_activity(void)
{
    ef_activity_object *obj = (ef_activity_object *)(guest + 0x600);
    memset(obj, 0, sizeof(*obj));
    obj->selected_category = 3;
    obj->slots[3].code = 14;
    obj->slots[3].strength = 2;
    obj->slots[7].code = 14;
    obj->slots[7].strength = -1;
    obj->slots[8].code = 14;
    obj->slots[8].strength = 0;
    obj->slots[9].code = 13;
    obj->slots[9].strength = 1;
    assert(count_matching_activity_slots(0x600) == 2);
    obj->selected_category = 9;
    assert(count_matching_activity_slots(0x600) == 1);
}

static void test_scan(void)
{
    memset(g_active_unit_records, 0, sizeof(g_active_unit_records));
    g_active_unit_record_count = 3;
    g_active_unit_records[0].active_code = 0;
    g_active_unit_records[0].index = 51;
    g_active_unit_records[1].active_code = 1;
    g_active_unit_records[1].index = -23;
    g_active_unit_records[2].active_code = 1;
    g_active_unit_records[2].index = 17;
    assert(active_unit_record_contains(17));
    assert(active_unit_record_contains(-23));
    assert(!active_unit_record_contains(51));
    g_active_unit_record_count = 0;
    assert(!active_unit_record_contains(17));
}

static void test_round_robin(void)
{
    g_round_robin_count = 3;
    g_round_robin_cursor = 0;
    g_round_robin_ids[0] = 71;
    g_round_robin_ids[1] = 72;
    g_round_robin_ids[2] = 73;
    assert(next_round_robin_record() == 72);
    assert(next_round_robin_record() == 73);
    assert(next_round_robin_record() == 71);
    g_round_robin_count = 0;
    assert(next_round_robin_record() == 0xffffu);
    assert(g_round_robin_cursor == 0);
}

static void test_reset(void)
{
    unsigned i;
    g_reset_flag_a = g_reset_flag_b = g_reset_flag_c = g_reset_flag_d = 9;
    for (i = 0; i < 2; ++i) {
        g_reset_accumulator_a[i] = 17;
        g_reset_accumulator_b[i] = 23;
        g_reset_accumulator_c[i] = 31;
    }
    clear_runtime_transient_counters();
    assert(!g_reset_flag_a && !g_reset_flag_b && !g_reset_flag_c && !g_reset_flag_d);
    for (i = 0; i < 2; ++i) {
        assert(!g_reset_accumulator_a[i] && !g_reset_accumulator_b[i] && !g_reset_accumulator_c[i]);
    }
}

int main(void)
{
    test_reference_mirror();
    test_normalize();
    test_ring();
    test_lines();
    test_activity();
    test_scan();
    test_round_robin();
    test_reset();
    return 0;
}
