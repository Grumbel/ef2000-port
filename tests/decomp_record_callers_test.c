/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

static uint8_t guest[0x230000];
float g_short_to_float_scale;
ef_addr32 g_world_grid_base;
uint16_t g_panel_sequence_cursor;
ef_addr32 g_last_transform_child;
double g_transform_scale_a, g_transform_scale_b, g_transform_scale_c, g_transform_scale_d;

static ef_addr32 lookup_result;
static int16_t lookup_requested;
static unsigned lookup_calls;
static ef_addr32 last_clock;
static int16_t last_band;
static unsigned clear_calls;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    lookup_requested = index;
    ++lookup_calls;
    return lookup_result;
}

uint8_t reference_clock_reaches(ef_addr32 clock)
{
    last_clock = clock;
    return (uint8_t)(clock & 1);
}

int32_t clear_panel_strip(int16_t band)
{
    ++clear_calls;
    last_band = band;
    return band * 3;
}

static void put16(ef_addr32 address, uint16_t value)
{
    memcpy(guest + address, &value, 2);
}

static uint16_t get16(ef_addr32 address)
{
    uint16_t value;
    memcpy(&value, guest + address, 2);
    return value;
}

static void put32(ef_addr32 address, uint32_t value)
{
    memcpy(guest + address, &value, 4);
}

static uint32_t get32(ef_addr32 address)
{
    uint32_t value;
    memcpy(&value, guest + address, 4);
    return value;
}

static double get_double(ef_addr32 address)
{
    double value;
    memcpy(&value, guest + address, 8);
    return value;
}

static void test_short_scale(void)
{
    g_short_to_float_scale = 0.25f;
    assert(convert_signed_short_scale(-104) == -26.0);
    assert(convert_signed_short_scale(32767) == 8191.75);
}

static void test_clock_row(void)
{
    ef_world_grid_cell *row;
    g_world_grid_base = 0x21000;
    row = (ef_world_grid_cell *)(guest + g_world_grid_base + 2 * 0xca);
    row->clock_base = 0x18000;
    row->activity_countdown = 3;
    assert(grid_clock_reaches_reference(2) == ((0x18000 + 3 * 25 + 16) & 1));
    assert(last_clock == 0x18000 + 3 * 25 + 16);
    row->activity_countdown = 0;
    assert(grid_clock_reaches_reference(2) == 0);
    assert(last_clock == 0x18010);
}

static void test_child_index(void)
{
    ef_addr32 parent = 0x23000;
    ef_addr32 child = 0x24000;
    lookup_calls = 0;
    put16(parent + 0x56, 42);
    refresh_object_child_slot(0);
    assert(get16(parent + 0x56) == 42);
    put32(parent + 0x30, 0);
    refresh_object_child_slot(parent);
    assert(get16(parent + 0x56) == 0xff);
    assert(lookup_calls == 0);

    put32(parent + 0x30, child);
    put16(child + 0x16, 0xffe8);
    lookup_result = child;
    refresh_object_child_slot(parent);
    assert(lookup_requested == -24 && lookup_calls == 1);
    assert(get16(parent + 0x56) == 0xffe8);
    lookup_result = 0x25000;
    refresh_object_child_slot(parent);
    assert(get16(parent + 0x56) == 0xff);
    assert(lookup_calls == 2);
}

static void test_slot_queries(void)
{
    ef_addr32 ptr = 0x26000;
    ef_activity_object *obj = (ef_activity_object *)(guest + ptr);
    memset(obj, 0, sizeof(*obj));
    lookup_result = ptr;
    obj->slots[0].code = 0x21;
    obj->slots[0].strength = 3;
    obj->slots[1].code = 0x22;
    obj->slots[1].strength = -2;
    obj->slots[2].code = 0x55;
    obj->slots[2].strength = 0;
    obj->slots[3].code = 0x55;
    obj->slots[3].strength = -7;
    obj->slots[4].code = 0x81;
    obj->slots[4].strength = 1;
    assert(first_nonignored_activity_slot(-11) == 3);
    assert(lookup_requested == -11);
    assert(query_indexed_object_feature(10, 0x55) == 3);
    assert(lookup_requested == 10);
    assert(query_indexed_object_feature(10, 0x81) == 20);
    obj->slots[3].strength = 0;
    assert(first_nonignored_activity_slot(10) == 4);
    obj->slots[4].strength = 0;
    assert(first_nonignored_activity_slot(10) == 20);
    lookup_result = 0;
    assert(first_nonignored_activity_slot(10) == 20);
}

static void test_panel_sequence(void)
{
    clear_calls = 0;
    g_panel_sequence_cursor = 0;
    assert(advance_panel_strip() == 1);
    assert(clear_calls == 1 && last_band == 1);
    assert(get16(0x1fc8a2 + 12) == 1);
    g_panel_sequence_cursor = 15;
    assert(advance_panel_strip() == 16);
    assert(last_band == 0);
    assert(get16(0x1fc8a2) == 16);
    g_panel_sequence_cursor = 0xfffe;
    assert(advance_panel_strip() == 0xffff);
    assert(last_band == -1);
    assert(get16(0x1fc8a2 - 12) == 0xffff);
}

static void test_child_transform(void)
{
    const ef_addr32 parent = 0x27000, child = 0x28000, output = 0x29000;
    put32(parent + 0x30, 0);
    g_last_transform_child = 0;
    put32(output, 0x12345678);
    transform_child_coordinates(parent, output);
    assert(g_last_transform_child == 0);
    assert(get32(output) == 0x12345678);
    put32(parent + 0x30, child);
    put32(child, 10);
    put32(child + 4, 0x80000000u);
    put32(child + 8, 30);
    g_transform_scale_a = 0.5;
    g_transform_scale_b = 2.0;
    transform_child_coordinates(parent, output);
    assert(g_last_transform_child == child);
    assert(guest[parent + 0x5a] == 1);
    assert(get_double(output) == 30.0);
    assert(get_double(output + 8) == 10.0);
    assert(get_double(output + 16) == -2147483648.0);
    guest[parent + 0x5a] = 0;
    put32(child + 4, 15);
    transform_child_coordinates(parent, output);
    assert(guest[parent + 0x5a] == 0);
    assert(get_double(output + 16) == 15.0);
}

static void test_angles(void)
{
    const ef_addr32 object = 0x2a000, a = 0x2b000, b = 0x2b020;
    put16(object + 0xc, 0xff80);
    put16(object + 0xe, 300);
    g_transform_scale_c = 0.25;
    g_transform_scale_d = 4.0;
    transform_signed_angle_pair(object, a, b);
    assert(get_double(a) == 128.0 && get_double(b) == 300.0);
    put16(object + 0xc, 24);
    put16(object + 0xe, 0xfffe);
    transform_signed_angle_pair(object, a, b);
    assert(get_double(a) == -24.0 && get_double(b) == -2.0);
}

int main(void)
{
    test_short_scale();
    test_clock_row();
    test_child_index();
    test_slot_queries();
    test_panel_sequence();
    test_child_transform();
    test_angles();
    return 0;
}
