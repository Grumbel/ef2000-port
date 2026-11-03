/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Asset-free C models of seven draft functions, not game differential proof. */
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define GRID_ADDRESS 0x002b0000u
#define ADJACENCY_ADDRESS 0x002c0000u
#define OBJECT_A_ADDRESS 0x00300000u
#define OBJECT_B_ADDRESS 0x00301000u
#define NODE_A_ADDRESS 0x002d0000u
#define NODE_B_ADDRESS 0x002d0100u

ef_addr32 g_world_grid_base;
ef_addr32 g_grid_adjacency_base;
ef_addr32 g_grid_node_head;
ef_addr32 g_grid_node_tail;
ef_addr32 g_grid_node_free;
uint32_t g_grid_secondary_selector;
ef_grid_category_type g_grid_category_types[256];
ef_grid_external_probe g_grid_external_probes[8];
double g_grid_reference_position[2];
double g_grid_visibility_distance;
int16_t g_active_grid_index;
uint8_t g_world_grid_mode;
uint8_t g_external_grid_count;

static ef_world_grid_cell grid[12];
static ef_grid_adjacency adjacency[8];
static ef_grid_pool_node nodes[2];
static uint8_t objects[2][0x220];
static ef_addr32 objects_by_index[4];
static unsigned lookup_calls;
static unsigned reference_calls;
static unsigned external_calls;
static uint8_t reference_near;
static uint8_t external_near[8];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    switch (address) {
    case GRID_ADDRESS: return grid;
    case ADJACENCY_ADDRESS: return adjacency;
    case OBJECT_A_ADDRESS: return objects[0];
    case OBJECT_B_ADDRESS: return objects[1];
    case NODE_A_ADDRESS: return &nodes[0];
    case NODE_B_ADDRESS: return &nodes[1];
    default: assert(0 && "unmapped guest test address"); return 0;
    }
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    ++lookup_calls;
    if (index >= 0 && index < 4) return objects_by_index[index];
    return 0;
}

uint8_t grid_reference_distance_matches(int16_t cell)
{
    assert(cell == 1);
    ++reference_calls;
    return reference_near;
}

uint8_t grid_external_distance_matches(int16_t cell, uint8_t probe)
{
    assert(cell == 1 && probe < 8);
    ++external_calls;
    return external_near[probe];
}

static void write_word(uint8_t *bytes, size_t offset, int16_t value)
{
    uint16_t bits = (uint16_t)value;
    bytes[offset] = (uint8_t)bits;
    bytes[offset + 1] = (uint8_t)(bits >> 8);
}

static void set_attached(int16_t cell, unsigned slot, int16_t index)
{
    write_word((uint8_t *)&grid[cell], 0x50 + slot * 2, index);
}

static void reset_fixture(void)
{
    size_t i;
    memset(grid, 0, sizeof(grid));
    memset(adjacency, 0, sizeof(adjacency));
    memset(nodes, 0, sizeof(nodes));
    memset(objects, 0, sizeof(objects));
    memset(objects_by_index, 0, sizeof(objects_by_index));
    memset(g_grid_category_types, 0, sizeof(g_grid_category_types));
    memset(external_near, 0, sizeof(external_near));
    g_world_grid_base = GRID_ADDRESS;
    g_grid_adjacency_base = ADJACENCY_ADDRESS;
    g_grid_node_head = 0;
    g_grid_node_tail = 0;
    g_grid_node_free = 0;
    g_grid_secondary_selector = 0;
    g_active_grid_index = 0xff;
    g_world_grid_mode = 0;
    g_external_grid_count = 0;
    lookup_calls = reference_calls = external_calls = 0;
    reference_near = 0;
    objects_by_index[1] = OBJECT_A_ADDRESS;
    objects_by_index[2] = OBJECT_B_ADDRESS;
    for (i = 0; i < 8; ++i) adjacency[2].neighbors[i] = 0xff;
    for (i = 0; i < 7; ++i) set_attached(1, (unsigned)i, -1);
}

static void test_pool(void)
{
    reset_fixture();
    g_grid_node_head = NODE_A_ADDRESS;
    g_grid_node_tail = NODE_A_ADDRESS;
    g_grid_node_free = NODE_B_ADDRESS;
    nodes[0].next = NODE_B_ADDRESS;
    recycle_oldest_grid_node();
    assert(g_grid_node_head == 0 && g_grid_node_tail == 0);
    assert(g_grid_node_free == NODE_A_ADDRESS && nodes[0].next == NODE_B_ADDRESS);

    reset_fixture();
    g_grid_node_head = NODE_A_ADDRESS;
    g_grid_node_tail = NODE_B_ADDRESS;
    nodes[0].next = NODE_B_ADDRESS;
    recycle_oldest_grid_node();
    assert(g_grid_node_head == NODE_B_ADDRESS && g_grid_node_tail == NODE_B_ADDRESS);
    assert(g_grid_node_free == NODE_A_ADDRESS && nodes[0].next == 0);
}

static void test_neighbor_and_distance(void)
{
    reset_fixture();
    grid[1].adjacency_group = 2;
    grid[0].adjacency_group = 2;
    adjacency[2].neighbors[0] = 1; /* Ignore self. */
    adjacency[2].neighbors[1] = 3;
    adjacency[2].neighbors[2] = 4;
    grid[3].detail_mode = 2;
    grid[4].detail_mode = 1;
    grid[4].current_mode = 2;
    assert(find_grid_neighbor_with_detail(1) == 4);
    g_active_grid_index = 0;
    grid[1].position_a = 3.0;
    grid[1].position_b = 4.0;
    grid[4].position_a = 0.0;
    grid[4].position_b = 0.0;
    assert(grid_cell_distance(1, 4) == 5.0);
    assert(grid_neighbor_distance(1) == 5.0);
    grid[4].current_mode = 0;
    assert(grid_neighbor_distance(1) == 0.0);
    grid[4].current_mode = 2;
    grid[0].adjacency_group = 1;
    assert(grid_neighbor_distance(1) == 0.0);
    grid[0].adjacency_group = 2;
    g_active_grid_index = 0xff;
    assert(grid_neighbor_distance(1) == 0.0);
    g_active_grid_index = 0;
    adjacency[2].neighbors[2] = 0xff;
    assert(find_grid_neighbor_with_detail(1) == 0xff);
    assert(grid_neighbor_distance(1) == 0.0);
    grid[1].position_a = 13.0;
    assert(grid_cell_distance(1, 4) == sqrt(185.0));
    grid[1].position_a = NAN;
    assert(isnan(grid_cell_distance(1, 4)));
}

static void test_cross_object_relation(void)
{
    reset_fixture();
    set_attached(1, 0, 1);
    write_word(objects[0], 0x146, 2); /* 0x144 high word */
    write_word(objects[0], 0x28, 3);  /* 0x26 high word */
    write_word(objects[1], 0x28, 4);
    grid[3].locator_packed = 7 << 16;
    grid[4].locator_packed = 8 << 16;
    assert(grid_has_cross_object_relation(1) == 1);
    assert(lookup_calls == 2);
    lookup_calls = 0;
    grid[4].locator_packed = grid[3].locator_packed;
    assert(grid_has_cross_object_relation(1) == 0);
    assert(lookup_calls == 8);
    lookup_calls = 0;
    objects_by_index[2] = 0;
    assert(grid_has_cross_object_relation(1) == 0);
    assert(lookup_calls == 8);
}

static void set_feature(unsigned slot, uint8_t type, uint16_t present)
{
    uint16_t offset = (uint16_t)(0x19b + slot * 3);
    objects[0][offset] = type;
    write_word(objects[0], offset + 1, (int16_t)present);
}

static void test_category_selection(void)
{
    reset_fixture();
    set_attached(1, 0, 1);
    g_grid_category_types[15].allowed_kind = 2;
    g_grid_category_types[12].allowed_kind = 2;
    g_grid_category_types[6].allowed_kind = 2;
    g_grid_category_types[7].allowed_kind = 2;
    set_feature(0, 15, 1);
    set_feature(1, 12, 1);
    set_feature(2, 6, 1);
    set_feature(3, 7, 1);
    refresh_grid_category(1);
    assert(grid[1].category_code == 6 && lookup_calls == 1);

    lookup_calls = 0;
    set_feature(2, 6, 0);
    refresh_grid_category(1);
    assert(grid[1].category_code == 7 && lookup_calls == 1);

    lookup_calls = 0;
    g_grid_category_types[7].allowed_kind = 1;
    refresh_grid_category(1);
    assert(grid[1].category_code == 12 && lookup_calls == 1);

    lookup_calls = 0;
    g_grid_secondary_selector = 1u << 16;
    set_attached(1, 1, 3); /* Switch to +0x52 if unsigned +0x50 equals 1. */
    refresh_grid_category(1);
    assert(grid[1].category_code == 12 && lookup_calls == 1);

    lookup_calls = 0;
    g_grid_secondary_selector = 0;
    objects_by_index[1] = 0;
    grid[1].category_code = 0xaa;
    refresh_grid_category(1);
    assert(grid[1].category_code == 0xaa && lookup_calls == 1);

    /* The left-hand unsigned 16-bit index 0xffff must NOT equal a
     * sign-extended -1 in the high word of the selector. */
    reset_fixture();
    set_attached(1, 0, -1);
    set_attached(1, 1, 2);
    g_grid_secondary_selector = 0xffff0000u;
    grid[1].category_code = 0xaa;
    refresh_grid_category(1);
    assert(grid[1].category_code == 0xaa && lookup_calls == 1);

    lookup_calls = 0;
    refresh_grid_category(0xff);
    assert(lookup_calls == 0);
}

static void test_visibility(void)
{
    reset_fixture();
    assert(grid_is_visible_from_reference(0xff) == 0);
    assert(reference_calls == 0);
    grid[1].current_mode = 3;
    reference_near = 1;
    assert(grid_is_visible_from_reference(1) == 1);
    assert(reference_calls == 1 && external_calls == 0);

    reference_near = 0;
    g_world_grid_mode = 1;
    g_external_grid_count = 3;
    external_near[0] = 1;
    external_near[2] = 1;
    assert(grid_is_visible_from_reference(1) == 1);
    assert(reference_calls == 2 && external_calls == 3);
    grid[1].current_mode = 0;
    assert(grid_is_visible_from_reference(1) == 0);
    assert(reference_calls == 3 && external_calls == 6);
    grid[1].current_mode = 3;
    external_near[0] = external_near[2] = 0;
    assert(grid_is_visible_from_reference(1) == 0);
    assert(external_calls == 9);
}

int main(void)
{
    test_pool();
    test_neighbor_and_distance();
    test_cross_object_relation();
    test_category_selection();
    test_visibility();
    return 0;
}
