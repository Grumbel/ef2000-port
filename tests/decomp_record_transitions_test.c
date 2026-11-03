/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

static uint8_t memory[0x230000];
uint8_t g_tactical_display_mode, g_tactical_marker_code;
int16_t g_tactical_coordinate_offset_x, g_tactical_coordinate_offset_y;
uint32_t g_position_bin_packed;
uint8_t g_tactical_action_enabled;
ef_addr32 g_ui_record_head, g_ui_record_anchor, g_ui_record_end, g_ui_record_cursor;
uint16_t g_ui_record_pool_count;
uint32_t g_ui_record_pool_pointer;

static uint32_t last_draw_mode, last_draw_aux;
static int32_t last_draw_x, last_draw_y, last_draw_result;
static uint8_t last_draw_mark;
static ef_addr32 last_draw_item;
static unsigned draw_calls, metric_calls, bin_calls, notify_calls, check_calls;
static unsigned init_calls, copy_calls, append_calls;
static int32_t last_bin_argument, last_event_index;
static uint32_t last_event_code, record_check_result;
static ef_addr32 last_check_record;
static ef_addr32 string_destinations[3], string_sources[3];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static void put16(ef_addr32 address, uint16_t value)
{
    memory[address] = (uint8_t)value;
    memory[address + 1] = (uint8_t)(value >> 8);
}

static void put32(ef_addr32 address, uint32_t value)
{
    put16(address, (uint16_t)value);
    put16(address + 2u, (uint16_t)(value >> 16));
}

static uint32_t get32(ef_addr32 address)
{
    return (uint32_t)memory[address] | ((uint32_t)memory[address + 1] << 8) |
           ((uint32_t)memory[address + 2] << 16) | ((uint32_t)memory[address + 3] << 24);
}

void draw_tactical_coordinate_marker(uint8_t mark, uint32_t mode,
                                     int32_t x, int32_t y, uint32_t aux)
{
    last_draw_mark = mark;
    last_draw_mode = mode;
    last_draw_x = x;
    last_draw_y = y;
    last_draw_aux = aux;
    ++draw_calls;
}

int32_t draw_tactical_mode_item(uint32_t mode, ef_addr32 item)
{
    last_draw_mode = mode;
    last_draw_item = item;
    ++metric_calls;
    return last_draw_result;
}

uint16_t bin_coordinate_to_slot(int32_t coordinate)
{
    last_bin_argument = coordinate;
    ++bin_calls;
    return (uint16_t)(coordinate < 0 ? -5 : coordinate + 2);
}

void notify_tactical_action(int32_t index, uint32_t action)
{
    last_event_index = index;
    last_event_code = action;
    ++notify_calls;
}

void guest_string_copy(ef_addr32 dest, ef_addr32 src)
{
    string_destinations[copy_calls] = dest;
    string_sources[copy_calls] = src;
    ++copy_calls;
}

void guest_string_append(ef_addr32 dest, ef_addr32 src)
{
    string_destinations[copy_calls + append_calls] = dest;
    string_sources[copy_calls + append_calls] = src;
    ++append_calls;
}

uint32_t original_record_selection_check(ef_addr32 record)
{
    last_check_record = record;
    ++check_calls;
    return record_check_result;
}

void initialize_ui_record_ring(ef_addr32 head)
{
    assert(head == g_ui_record_head);
    ++init_calls;
    g_ui_record_cursor = 0x30100;
}

static void test_tactical_display(void)
{
    g_tactical_coordinate_offset_x = 7;
    g_tactical_coordinate_offset_y = -4;
    g_tactical_marker_code = 0x31;
    g_tactical_display_mode = 1;
    draw_tactical_coordinates(-3, 4, 0x11223344);
    assert(draw_calls == 1 && last_draw_mark == 0x31);
    assert(last_draw_mode == 0 && last_draw_x == 1 && last_draw_y == 4);
    assert(last_draw_aux == 0x11223344);
    g_tactical_display_mode = 7;
    draw_tactical_coordinates(-3, 4, 0x8899);
    assert(draw_calls == 2 && last_draw_mode == 1);
    assert(last_draw_x == 4 && last_draw_y == 0 && last_draw_aux == 0x8899);

    last_draw_result = 0x1234fffbu;
    g_tactical_display_mode = 1;
    assert(draw_tactical_item_metric(0x12345) == -3);
    assert(last_draw_mode == 0 && last_draw_item == 0x12345);
    g_tactical_display_mode = 7;
    assert((uint32_t)draw_tactical_item_metric(0x34567) == 0x1234fffbu);
    assert(last_draw_mode == 1 && last_draw_item == 0x34567 && metric_calls == 2);
}

static void test_position_and_action(void)
{
    g_position_bin_packed = 0xfffc1234u;
    update_position_bin();
    assert(bin_calls == 1 && last_bin_argument == -4);
    assert(g_position_bin_packed == 0xffff1234u);
    g_position_bin_packed = 0x00031234u;
    update_position_bin();
    assert(bin_calls == 2 && last_bin_argument == 3);
    assert(g_position_bin_packed == 0x00051234u);

    put32(0x20000 + 0x12, 0);
    put32(0x20000 + 0x1e, 0xffff0000u);
    g_tactical_action_enabled = 0;
    report_tactical_object_action(0x20000);
    assert(notify_calls == 0);
    g_tactical_action_enabled = 1;
    report_tactical_object_action(0x20000);
    assert(notify_calls == 1 && last_event_index == -1 && last_event_code == 0x26);
    put32(0x20000 + 0x12, 0x21000);
    report_tactical_object_action(0x20000);
    assert(notify_calls == 2 && last_event_code == 0x12);
}

static void test_text_and_selection(void)
{
    assert(build_status_description(0x1f00) == 0x1f8ec4);
    assert(copy_calls == 1 && append_calls == 2);
    assert(string_destinations[0] == 0x1f8ec4 && string_sources[0] == 0x1f8d60);
    assert(string_destinations[1] == 0x1f8ec4 && string_sources[1] == 0x187b74);
    assert(string_destinations[2] == 0x1f8ec4 && string_sources[2] == 0x1f00);

    put32(0x20000 + 0x12, 0x21000);
    put16(0x20000 + 0x16, 1);
    put16(0x21000 + 6, 0x1234);
    put32(0x21000 + 8, 0x41000);
    record_check_result = 1;
    refresh_record_selection(0x20000);
    assert(get32(0x20000 + 0x3c) == 0x41000);
    assert(memory[0x20000 + 0x40] == 0x34 && memory[0x20000 + 0x41] == 0x12);
    assert(last_check_record == 0x20000 && check_calls == 1);
    assert(memory[0x20000 + 0x16] == 2);

    put16(0x20000 + 0x16, 0);
    put16(0x21000, 0xfeed);
    put32(0x21000 + 2, 0);
    refresh_record_selection(0x20000);
    assert(get32(0x20000 + 0x3c) == 0 && check_calls == 1);
    assert(memory[0x20000 + 0x40] == 0xed);
}

static void test_ui_ring_and_pool(void)
{
    g_ui_record_head = 0x33000;
    g_ui_record_anchor = 0x30000;
    g_ui_record_end = 0x30300;
    g_ui_record_cursor = 0;
    put32(0x30100 + 0x34, 0x30200);
    put32(0x30000 + 0x34, 0x30300);
    put16(0x30100 + 0x54, 0xa2c3);
    rotate_ui_record_ring();
    assert(init_calls == 1 && g_ui_record_cursor == 0x30200);
    assert(g_ui_record_head == 0x30100);
    assert(get32(0x30100 + 0x34) == 0x30300);
    assert(get32(0x30300 + 0x38) == 0x30100);
    assert(get32(0x30100 + 0x38) == 0x30000);
    assert(get32(0x30000 + 0x34) == 0x30100);
    assert(memory[0x30100 + 0x51] == 0xc3 && memory[0x30100 + 0x52] == 0xa2);

    g_ui_record_pool_count = 17;
    g_ui_record_pool_pointer = 0x30000;
    memset(memory + 0x20f550, 0xfe, 0x2260);
    reset_ui_record_pool();
    assert(g_ui_record_pool_count == 0 && g_ui_record_pool_pointer == 0);
    for (unsigned i = 0; i < 0x2260; ++i) {
        assert(memory[0x20f550 + i] == 0);
    }
    assert(memory[0x20f550 - 1] == 0 && memory[0x20f550 + 0x2260] == 0);
}

int main(void)
{
    test_tactical_display();
    test_position_and_action();
    test_text_and_selection();
    test_ui_ring_and_pool();
    return 0;
}
