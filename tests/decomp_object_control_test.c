/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Branch and side-effect tests only: not an original-guest differential. */
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <assert.h>
#include <string.h>

#define OBJECT_ADDRESS 0x002a0000u
#define GRID_ADDRESS 0x002b0000u
#define SECOND_GRID_ADDRESS 0x002c0000u

int16_t g_indexed_object_count;
ef_addr32 g_indexed_object_table[8];
uint32_t g_filtered_object_lookup_calls;
int16_t g_excluded_object_index;
uint8_t g_object_type_flags[256 * 0xe5];
ef_addr32 g_world_grid_base;
ef_addr32 g_current_object;

static uint8_t object[0x170];
static ef_world_grid_cell grid[4];
static ef_world_grid_cell alternate_grid[4];
static int event_count;
static int16_t last_index;
static int16_t last_event;
static int32_t last_aux_b;
static int32_t last_aux_c;
static int32_t last_stack_arg;
static int move_grid_on_dispatch;
static int process29_count;
static int process30_count;
static uint8_t relation_result;
static int relation_checks;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    if (addr == OBJECT_ADDRESS) return object;
    if (addr == GRID_ADDRESS) return grid;
    if (addr == SECOND_GRID_ADDRESS) return alternate_grid;
    assert(0 && "unmapped guest address");
    return 0;
}

uint8_t check_current_object_relation(void)
{
    ++relation_checks;
    return relation_result;
}

void dispatch_indexed_object_event(int16_t index, int16_t event,
                                   int32_t aux_b, int32_t aux_c,
                                   int32_t stack_arg)
{
    ++event_count;
    last_index = index;
    last_event = event;
    last_aux_b = aux_b;
    last_aux_c = aux_c;
    last_stack_arg = stack_arg;
    if (move_grid_on_dispatch) g_world_grid_base = SECOND_GRID_ADDRESS;
}

void process_object_state_29(ef_addr32 address)
{
    assert(address == OBJECT_ADDRESS);
    ++process29_count;
}

void process_object_state_30(ef_addr32 address)
{
    assert(address == OBJECT_ADDRESS);
    ++process30_count;
}

static void reset_fixture(void)
{
    memset(object, 0x57, sizeof(object));
    memset(grid, 0, sizeof(grid));
    memset(alternate_grid, 0, sizeof(alternate_grid));
    memset(g_indexed_object_table, 0, sizeof(g_indexed_object_table));
    memset(g_object_type_flags, 0, sizeof(g_object_type_flags));
    g_indexed_object_count = 8;
    g_excluded_object_index = -1;
    g_indexed_object_table[1] = OBJECT_ADDRESS;
    g_filtered_object_lookup_calls = 0;
    g_world_grid_base = GRID_ADDRESS;
    g_current_object = OBJECT_ADDRESS;
    grid[2].indexed_object_packed = 1 << 16;
    object[0x28] = 2; /* high word of original object dword at +0x26 */
    object[0x29] = 0;
    event_count = 0;
    last_index = last_event = 0;
    last_aux_b = last_aux_c = last_stack_arg = 1;
    move_grid_on_dispatch = 0;
    process29_count = process30_count = 0;
    relation_result = 0;
    relation_checks = 0;
}

static void assert_event(int16_t index, int16_t event)
{
    assert(event_count == 1);
    assert(last_index == index);
    assert(last_event == event);
    assert(last_aux_b == 0 && last_aux_c == 0 && last_stack_arg == 0);
}

static void test_grid_object_membership(void)
{
    reset_fixture();
    assert(grid_slot_matches_object(1) == 1);
    assert(g_filtered_object_lookup_calls == 1);
    grid[2].indexed_object_packed = 0x00030000;
    assert(grid_slot_matches_object(1) == 0);
    assert(g_filtered_object_lookup_calls == 2);
    g_excluded_object_index = 1;
    assert(grid_slot_matches_object(1) == 0);
    assert(grid_slot_matches_object(-1) == 0);
    assert(g_filtered_object_lookup_calls == 4);
}

static void test_grid_event(void)
{
    reset_fixture();
    set_grid_slot_event(2, 0xf7);
    assert_event(1, 0x14f);
    assert(grid[2].event_value == 0xf7);

    reset_fixture();
    move_grid_on_dispatch = 1;
    set_grid_slot_event(2, 3);
    assert_event(1, 0x5b);
    assert(grid[2].event_value == 0);
    assert(alternate_grid[2].event_value == 3);
}

static void test_state_enable(void)
{
    reset_fixture();
    grid[2].object_type = 4;
    object[0xe9] = 4;
    g_object_type_flags[4 * 0xe5] = 1;
    grid[2].state_enabled = 0;
    enable_grid_slot_state(2);
    assert(grid[2].state_enabled == 1);
    assert(object[0x12a] == 1);
    assert(object[0x13d] == 3 && object[0x13e] == 8);
    assert_event(1, 7);
    assert(g_filtered_object_lookup_calls == 1);
    enable_grid_slot_state(2);
    assert(event_count == 1);

    reset_fixture();
    grid[2].object_type = 4;
    enable_grid_slot_state(2);
    assert(event_count == 0);
    assert(g_filtered_object_lookup_calls == 0);
    assert(grid[2].state_enabled == 0);
}

static void test_activation_enable(void)
{
    reset_fixture();
    object[0x140] = 1;
    grid[2].activation_requested = 1;
    enable_grid_slot_activation(2);
    assert(grid[2].activation_active == 1);
    assert(object[0x141] == 1 && object[0x142] == 0);
    assert_event(1, 8);
    assert(g_filtered_object_lookup_calls == 1);
    enable_grid_slot_activation(2);
    assert(event_count == 1);

    reset_fixture();
    grid[2].activation_requested = 0;
    enable_grid_slot_activation(2);
    assert(event_count == 0 && g_filtered_object_lookup_calls == 0);
    assert(grid[2].activation_active == 0);
}

static void test_object_mode(void)
{
    reset_fixture();
    object[0x16c] = 2;
    object[0x16e] = 0x57;
    set_indexed_object_mode(1, 1);
    assert(object[0x16d] == 1);
    assert(object[0x16e] == 0x57);
    assert(process29_count == 1 && process30_count == 1);
    set_indexed_object_mode(1, 2);
    assert(process29_count == 2 && process30_count == 2);
    set_indexed_object_mode(1, 0xff);
    assert(object[0x16d] == 0xff && object[0x16e] == 1);
    assert(process29_count == 2 && process30_count == 2);
    assert(g_filtered_object_lookup_calls == 3);

    object[0x16c] = 1;
    set_indexed_object_mode(1, 1);
    set_indexed_object_mode(3, 1);
    assert(process29_count == 2 && process30_count == 2);
    assert(g_filtered_object_lookup_calls == 5);
}

static void test_current_relation_sync(void)
{
    reset_fixture();
    object[0x146] = 0x12;
    object[0x147] = 0x34;
    object[0x158] = 0xab;
    object[0x159] = 0xcd;
    sync_current_object_slot();
    assert(object[0x146] == 0x12 && object[0x147] == 0x34);
    relation_result = 1;
    sync_current_object_slot();
    assert(object[0x146] == 0xab && object[0x147] == 0xcd);
    assert(relation_checks == 2);
}

int main(void)
{
    test_grid_object_membership();
    test_grid_event();
    test_state_enable();
    test_activation_enable();
    test_object_mode();
    test_current_relation_sync();
    return 0;
}
