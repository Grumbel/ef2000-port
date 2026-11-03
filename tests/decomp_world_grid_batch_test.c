/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Synthetic host behavior: no original guest differential is implied. */
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <assert.h>
#include <string.h>

#define OBJ1_ADDR 0x002a0000u
#define OBJ2_ADDR 0x002a1000u
#define GRID_ADDR 0x002b0000u
#define GRID2_ADDR 0x002c0000u

int16_t g_indexed_object_count;
ef_addr32 g_indexed_object_table[10];
uint32_t g_filtered_object_lookup_calls;
int16_t g_excluded_object_index;
ef_addr32 g_world_grid_base;
uint8_t g_world_grid_mode;
uint8_t g_world_grid_marker;
uint8_t g_external_grid_count;
int16_t g_active_grid_index;

static ef_world_grid_cell grid[8];
static ef_world_grid_cell alternate_grid[8];
static uint8_t obj1[0x1a0];
static uint8_t obj2[0x1a0];
static int link_set_calls;
static int link_position_calls;
static int16_t last_link_cell;
static int16_t last_link_mode;
static int external_calls;
static int external_hit_at;
static int reset_calls;
static int visual_calls;
static int move_grid_on_reset;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    if (address == GRID_ADDR) return grid;
    if (address == GRID2_ADDR) return alternate_grid;
    if (address == OBJ1_ADDR) return obj1;
    if (address == OBJ2_ADDR) return obj2;
    assert(0 && "unmapped guest memory");
    return 0;
}

void set_grid_link_state(int16_t cell, int16_t mode)
{
    ++link_set_calls;
    last_link_cell = cell;
    last_link_mode = mode;
    assert(grid[cell].forward_link == 2);
    assert(grid[cell].link_state == 0);
}

void copy_grid_link_position(int16_t cell)
{
    ++link_position_calls;
    assert(link_set_calls == link_position_calls);
    assert(cell == last_link_cell);
}

uint8_t external_grid_link_matches(int16_t cell, uint8_t external_index)
{
    assert(cell == 3);
    assert(external_index == external_calls);
    ++external_calls;
    return (uint8_t)(external_calls == external_hit_at);
}

void reset_grid_cell_derived(int16_t cell)
{
    assert(cell == 1);
    assert(grid[1].reset_state == 0);
    ++reset_calls;
    if (move_grid_on_reset) g_world_grid_base = GRID2_ADDR;
}

void update_grid_visual_state(int16_t cell)
{
    assert(cell == 1);
    ++visual_calls;
    assert((g_world_grid_base == GRID2_ADDR ? alternate_grid : grid)[1].retained_a ==
           (g_world_grid_base == GRID2_ADDR ? alternate_grid : grid)[1].pending_a);
}

static void reset_fixture(void)
{
    memset(grid, 0, sizeof(grid));
    memset(alternate_grid, 0, sizeof(alternate_grid));
    memset(obj1, 0, sizeof(obj1));
    memset(obj2, 0, sizeof(obj2));
    memset(g_indexed_object_table, 0, sizeof(g_indexed_object_table));
    g_indexed_object_count = 10;
    g_indexed_object_table[1] = OBJ1_ADDR;
    g_indexed_object_table[2] = OBJ2_ADDR;
    g_excluded_object_index = -1;
    g_filtered_object_lookup_calls = 0;
    g_world_grid_base = GRID_ADDR;
    g_world_grid_mode = 0;
    g_world_grid_marker = 0x61;
    g_external_grid_count = 0;
    g_active_grid_index = -1;
    link_set_calls = link_position_calls = 0;
    last_link_cell = last_link_mode = -1;
    external_calls = external_hit_at = 0;
    reset_calls = visual_calls = move_grid_on_reset = 0;
}

static void set_attachment(int cell, int entry, int16_t index)
{
    uint8_t *raw = (uint8_t *)&grid[cell];
    uint32_t offset = 0x50u + (uint32_t)entry * 2u;
    raw[offset] = (uint8_t)index;
    raw[offset + 1] = (uint8_t)((uint16_t)index >> 8);
}

static void test_active_link_scan(void)
{
    reset_fixture();
    set_attachment(3, 0, 1);
    set_attachment(3, 1, 2);
    set_attachment(3, 2, -1);
    set_attachment(3, 3, 0xff);
    obj1[0x18d] = 1;
    assert(grid_has_active_link(3) == 1);
    assert(g_filtered_object_lookup_calls == 7); /* scans after match */
    obj1[0x18d] = 0;
    assert(grid_has_active_link(3) == 0);
    assert(g_filtered_object_lookup_calls == 14);
}

static void test_mode_scan(void)
{
    reset_fixture();
    ((uint8_t *)&grid[3])[0x4f] = 3;
    set_attachment(3, 0, 1);
    set_attachment(3, 1, 2);
    set_attachment(3, 2, 0xff);
    obj1[0x16c] = 4;
    obj2[0x16c] = 4;
    obj2[0x24] = 0x08;
    assert(grid_has_object_mode(3, 4) == 1);
    assert(g_filtered_object_lookup_calls == 3);
    obj1[0x24] = 0x08;
    assert(grid_has_object_mode(3, 4) == 0);
    assert(g_filtered_object_lookup_calls == 6);
    ((uint8_t *)&grid[3])[0x4f] = 0;
    assert(grid_has_object_mode(3, 4) == 0);
    assert(g_filtered_object_lookup_calls == 6);
}

static void test_grid_link(void)
{
    reset_fixture();
    grid[1].link_state = 9;
    grid[1].reverse_link = 44;
    link_grid_cell(1, 2);
    assert(grid[1].link_state == 0);
    assert(grid[1].forward_link == 2);
    assert(grid[1].reverse_link == 44);
    assert(grid[2].reverse_link == 1);
    assert(link_set_calls == 1 && link_position_calls == 1);
    assert(last_link_cell == 1 && last_link_mode == 3);

    reset_fixture();
    g_world_grid_mode = 1;
    grid[2].unknown_00[0] = 0;
    link_grid_cell(1, 2);
    assert(grid[2].reverse_link == 0);

    reset_fixture();
    g_world_grid_mode = 1;
    grid[2].unknown_00[0] = g_world_grid_marker;
    link_grid_cell(1, 2);
    assert(grid[2].reverse_link == 1);
}

static void test_current_or_external(void)
{
    reset_fixture();
    g_active_grid_index = 3;
    assert(grid_is_current_or_external(0xff) == 0);
    assert(grid_is_current_or_external(3) == 1);
    assert(grid_is_current_or_external(4) == 0);
    assert(external_calls == 0);

    reset_fixture();
    g_world_grid_mode = 1;
    g_external_grid_count = 4;
    external_hit_at = 3;
    assert(grid_is_current_or_external(3) == 1);
    assert(external_calls == 3); /* short-circuits the external helper */
    external_calls = 0;
    external_hit_at = 0;
    assert(grid_is_current_or_external(3) == 0);
    assert(external_calls == 4);
}

static void test_reset_grid_state(void)
{
    reset_fixture();
    grid[1].reset_state = 5;
    grid[1].pending_a = 0x12345678u;
    grid[1].pending_b = 0xaabbccddu;
    reset_grid_cell_state(1);
    assert(grid[1].reset_state == 0);
    assert(grid[1].retained_a == 0x12345678u);
    assert(grid[1].retained_b == 0xaabbccddu);
    assert(reset_calls == 1 && visual_calls == 0);
    reset_grid_cell_state(0xff);
    assert(reset_calls == 1);

    reset_fixture();
    grid[1].reset_state = 6;
    alternate_grid[1].pending_a = 0x87654321u;
    alternate_grid[1].pending_b = 0x1234abcdu;
    g_world_grid_mode = 1;
    move_grid_on_reset = 1;
    reset_grid_cell_state(1);
    assert(grid[1].reset_state == 0);
    assert(grid[1].retained_a == 0);
    assert(alternate_grid[1].retained_a == 0x87654321u);
    assert(alternate_grid[1].retained_b == 0x1234abcdu);
    assert(reset_calls == 1 && visual_calls == 1);
}

static void test_event_truth_table(void)
{
    int code, mode;
    for (code = 0; code < 256; ++code) {
        for (mode = 0; mode < 256; ++mode) {
            uint8_t expect = (uint8_t)((code == 0 && mode == 1) ||
                                      (code == 1 && mode == 0) ||
                                      (code == 2 && mode != 2));
            assert(grid_event_matches((uint8_t)code, (uint8_t)mode) == expect);
        }
    }
}

int main(void)
{
    test_active_link_scan();
    test_mode_scan();
    test_grid_link();
    test_current_or_external();
    test_reset_grid_state();
    test_event_truth_table();
    return 0;
}
