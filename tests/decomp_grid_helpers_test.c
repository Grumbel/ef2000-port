/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Asset-free host model, never a claim of original-game differential proof. */
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define GRID_ADDR 0x002b0000u
#define EXTERNAL_ADDR 0x002c0000u
#define OBJECT1_ADDR 0x002a0000u
#define OBJECT2_ADDR 0x002a1000u

ef_addr32 g_world_grid_base;
ef_addr32 g_external_grid_base;
uint8_t g_world_grid_mode;
uint8_t g_world_grid_marker;
int16_t g_active_grid_index;
double g_external_grid_scale;
double g_grid_limit_scale;
ef_grid_type_record g_grid_type_records[256];
int16_t g_indexed_object_count;
ef_addr32 g_indexed_object_table[8];
uint32_t g_filtered_object_lookup_calls;
int16_t g_excluded_object_index;

static ef_world_grid_cell grid[8];
static ef_external_grid_node external[8];
static uint8_t object1[0x1a0];
static uint8_t object2[0x1a0];
static unsigned state_calls;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    if (address == GRID_ADDR) return grid;
    if (address == EXTERNAL_ADDR) return external;
    if (address == OBJECT1_ADDR) return object1;
    if (address == OBJECT2_ADDR) return object2;
    assert(0 && "unmapped guest address");
    return 0;
}

void set_grid_link_state(int16_t cell, int16_t mode)
{
    assert(cell == 1 && mode == 3);
    assert(grid[1].link_state == 1);
    assert(grid[1].forward_link == 2);
    ++state_calls;
}

static void reset_fixture(void)
{
    memset(grid, 0, sizeof(grid));
    memset(external, 0, sizeof(external));
    memset(g_grid_type_records, 0, sizeof(g_grid_type_records));
    memset(g_indexed_object_table, 0, sizeof(g_indexed_object_table));
    memset(object1, 0, sizeof(object1));
    memset(object2, 0, sizeof(object2));
    g_world_grid_base = GRID_ADDR;
    g_external_grid_base = EXTERNAL_ADDR;
    g_world_grid_mode = 0;
    g_world_grid_marker = 0x61;
    g_active_grid_index = -1;
    g_external_grid_scale = 1.25;
    g_grid_limit_scale = 10.0;
    g_indexed_object_count = 8;
    g_excluded_object_index = -1;
    g_filtered_object_lookup_calls = 0;
    g_indexed_object_table[1] = OBJECT1_ADDR;
    g_indexed_object_table[2] = OBJECT2_ADDR;
    state_calls = 0;
}

static void test_grid_unlink(void)
{
    reset_fixture();
    grid[1].current_mode = 3;
    grid[1].link_state = 0;
    grid[1].forward_link = 2;
    grid[2].reverse_link = 1;
    detach_grid_link_on_mode_change(1, 4);
    assert(grid[2].reverse_link == 0xff);
    assert(grid[1].forward_link == 0xff);
    assert(grid[1].link_state == 0xff);

    reset_fixture();
    grid[1].current_mode = 3;
    grid[1].forward_link = 2;
    grid[2].reverse_link = 1;
    detach_grid_link_on_mode_change(1, 3);
    assert(grid[1].forward_link == 2 && grid[2].reverse_link == 1);

    reset_fixture();
    grid[1].current_mode = 3;
    grid[1].forward_link = 0xff;
    detach_grid_link_on_mode_change(1, 4);
    assert(grid[1].forward_link == 0xff && grid[1].link_state == 0);

    reset_fixture();
    grid[1].current_mode = 3;
    grid[1].link_state = 1;
    grid[1].forward_link = 2;
    grid[2].reverse_link = 1;
    detach_grid_link_on_mode_change(1, 4);
    assert(grid[1].forward_link == 2 && grid[2].reverse_link == 1);

    reset_fixture();
    grid[1].current_mode = 6;
    grid[1].detail_mode = 7;
    detach_grid_link_on_mode_change(1, 4);
    assert(grid[1].link_pending_marker == 0xff);
    reset_fixture();
    grid[1].current_mode = 6;
    grid[1].detail_mode = 6;
    g_active_grid_index = 1;
    detach_grid_link_on_mode_change(1, 4);
    assert(grid[1].link_pending_marker == 0);
}

static void test_linked_grid_position(void)
{
    reset_fixture();
    grid[1].forward_link = 2;
    grid[2].position_a = -9.75;
    grid[2].position_b = 0.125;
    grid[2].position_c = 65536.5;
    copy_grid_link_position(1);
    assert(grid[1].inherited_position_a == -9.75);
    assert(grid[1].inherited_position_b == 0.125);
    assert(grid[1].inherited_position_c == 65536.5);
}

static void test_external_link(void)
{
    reset_fixture();
    external[2].component_a = 10;
    external[2].component_b = -20;
    external[2].component_c = 50;
    link_external_grid_cell(1, 2);
    assert(grid[1].link_state == 1 && grid[1].forward_link == 2);
    assert(external[2].back_reference == 1);
    assert(state_calls == 1);
    assert(grid[1].inherited_position_a == 62.5);
    assert(grid[1].inherited_position_b == 12.5);
    assert(grid[1].inherited_position_c == -25);

    reset_fixture();
    g_world_grid_mode = 1;
    external[2].marker = 0x22;
    link_external_grid_cell(1, 2);
    assert(external[2].back_reference == 0);
    reset_fixture();
    g_world_grid_mode = 1;
    external[2].marker = g_world_grid_marker;
    link_external_grid_cell(1, 2);
    assert(external[2].back_reference == 1);
}

static void set_index(int16_t cell, unsigned entry, int16_t index)
{
    uint8_t *bytes = (uint8_t *)&grid[cell];
    unsigned offset = 0x50 + entry * 2;
    bytes[offset] = (uint8_t)index;
    bytes[offset + 1] = (uint8_t)((uint16_t)index >> 8);
}

static void set_object_limit(uint8_t *object, double limit)
{
    memcpy(object + 0x177, &limit, sizeof(limit));
}

static double read_pending_limit(const ef_world_grid_cell *row)
{
    double value;
    memcpy(&value, &row->pending_a, sizeof(value));
    return value;
}

static void test_limit_recalculation(void)
{
    reset_fixture();
    grid[1].object_type = 2;
    g_grid_type_records[2].limit_a = 100;
    g_grid_type_records[2].limit_b = 40;
    g_grid_type_records[2].limit_multiplier = 2;
    set_index(1, 0, 1);
    set_index(1, 1, 2);
    set_index(1, 2, -1);
    set_object_limit(object1, 150.0);
    set_object_limit(object2, 70.0);
    recompute_grid_limit(1);
    assert(read_pending_limit(&grid[1]) == 70.0);
    assert(g_filtered_object_lookup_calls == 7);

    reset_fixture();
    grid[1].object_type = 2;
    g_grid_type_records[2].limit_a = 10;
    g_grid_type_records[2].limit_b = 40;
    g_grid_type_records[2].limit_multiplier = 2;
    recompute_grid_limit(1);
    assert(read_pending_limit(&grid[1]) == 70.0);
    assert(g_filtered_object_lookup_calls == 7);
    recompute_grid_limit(0xff);
    assert(g_filtered_object_lookup_calls == 7);

    reset_fixture();
    grid[1].object_type = 2;
    g_grid_type_records[2].limit_a = 100;
    set_index(1, 0, 1);
    set_index(1, 1, 2);
    set_object_limit(object1, NAN);
    set_object_limit(object2, 45.0);
    recompute_grid_limit(1);
    assert(read_pending_limit(&grid[1]) == 45.0);
    assert(g_filtered_object_lookup_calls == 7);
}

int main(void)
{
    test_grid_unlink();
    test_linked_grid_position();
    test_external_link();
    test_limit_recalculation();
    return 0;
}
