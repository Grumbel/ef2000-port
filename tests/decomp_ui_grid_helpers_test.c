/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

#define GUEST_START 0x001f0000u
#define GUEST_SIZE  0x20000u
static uint8_t memory[GUEST_SIZE];
static uint8_t low_memory[0x4000];
int16_t g_tactical_lookup_count;
uint8_t g_tactical_lookup_codes[256];
uint16_t g_ui_panel_defaults[5];
uint16_t g_ui_panel_zero_a[2];
uint16_t g_ui_panel_zero_b[2];
ef_addr32 g_ui_linked_list_head;
ef_addr32 g_ui_linked_list_sentinel;
ef_addr32 g_world_grid_base;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    if (addr < sizeof(low_memory)) { return low_memory + addr; }
    assert(addr >= GUEST_START && addr < GUEST_START + GUEST_SIZE);
    return memory + addr - GUEST_START;
}

static void put32(ef_addr32 addr, uint32_t value)
{
    memcpy(ef_guest_memory_ptr(addr), &value, sizeof(value));
}

static void put16(ef_addr32 addr, uint16_t value)
{
    memcpy(ef_guest_memory_ptr(addr), &value, sizeof(value));
}

static uint16_t get16(ef_addr32 addr)
{
    uint16_t value;
    memcpy(&value, ef_guest_memory_ptr(addr), sizeof(value));
    return value;
}

static void test_clock_helpers(void)
{
    put16(0x200, 1); put16(0x202, 2); put16(0x204, 3);
    assert(clock_words_to_seconds(0x200) == 3723);
    assert(clock_triplet_is_present(0x200));
    put16(0x200, 0xffff); put16(0x202, 0xffff); put16(0x204, 0xffff);
    assert(clock_words_to_seconds(0x200) == -3661);
    assert(!clock_triplet_is_present(0x200));
    put16(0x204, 0);
    assert(clock_triplet_is_present(0x200));
}

static void test_tactical_lookup(void)
{
    g_tactical_lookup_count = 0;
    assert(!tactical_lookup_contains(7));
    g_tactical_lookup_codes[0] = 7;
    g_tactical_lookup_codes[1] = 9;
    g_tactical_lookup_count = 2;
    assert(tactical_lookup_contains(7));
    assert(tactical_lookup_contains(9));
    assert(!tactical_lookup_contains(8));
    g_tactical_lookup_count = -1;
    assert(!tactical_lookup_contains(7));
}

static void test_grid_activity(void)
{
    ef_world_grid_cell *row;
    g_world_grid_base = 0x400;
    row = (ef_world_grid_cell *)ef_guest_memory_ptr(g_world_grid_base + 2 * 0xca);
    row->activity_countdown = 0;
    decrement_grid_activity(2);
    assert(row->activity_countdown == 0);
    row->activity_countdown = 2;
    decrement_grid_activity(2);
    assert(row->activity_countdown == 1);
    decrement_grid_activity(2);
    decrement_grid_activity(2);
    assert(row->activity_countdown == 0);
}

static void test_ui_panel(void)
{
    unsigned i;
    for (i = 0; i < 5; ++i) { g_ui_panel_defaults[i] = 0xff; }
    for (i = 0; i < 2; ++i) {
        g_ui_panel_zero_a[i] = 0xff;
        g_ui_panel_zero_b[i] = 0xff;
    }
    assert(reset_ui_panel_selection() == 5);
    for (i = 0; i < 5; ++i) { assert(g_ui_panel_defaults[i] == 1); }
    for (i = 0; i < 2; ++i) {
        assert(g_ui_panel_zero_a[i] == 0);
        assert(g_ui_panel_zero_b[i] == 0);
    }
}

static void test_ui_list(void)
{
    ef_addr32 head = 0x001f1000;
    ef_addr32 first = 0x001f1100;
    ef_addr32 second = 0x001f1200;
    ef_addr32 tail = 0x001f1300;
    g_ui_linked_list_head = head;
    g_ui_linked_list_sentinel = tail;
    put32(head + 0x34, first);
    put32(first + 0x34, second);
    put32(second + 0x34, tail);
    *(uint8_t *)ef_guest_memory_ptr(first + 0x50) = 0xa4;
    *(uint8_t *)ef_guest_memory_ptr(second + 0x50) = 0xa4;
    put16(first + 0x51, 5);
    put16(second + 0x51, 6);
    put16(first + 0x56, 18);
    put16(second + 0x56, 18);
    assert(find_ui_linked_tag(0xa4, 5) == first);
    assert(find_ui_linked_tag(0xa4, 6) == second);
    assert(find_ui_linked_tag(0xa4, 7) == 0);
    assert(find_ui_linked_tag(0xa5, 5) == 0);
    assert(mark_ui_links_by_slot(18) == tail);
    assert(*(uint8_t *)ef_guest_memory_ptr(first + 0x42) == 1);
    assert(*(uint8_t *)ef_guest_memory_ptr(second + 0x42) == 1);
    *(uint8_t *)ef_guest_memory_ptr(first + 0x42) = 0;
    assert(mark_ui_links_by_slot(19) == tail);
    assert(*(uint8_t *)ef_guest_memory_ptr(first + 0x42) == 0);
    put32(head + 0x34, tail);
    assert(find_ui_linked_tag(0xa4, 5) == 0);
    assert(mark_ui_links_by_slot(18) == tail);
}

static void test_panel_strips(void)
{
    unsigned i;
    for (i = 1; i <= 16; ++i) {
        *(uint8_t *)ef_guest_memory_ptr(0x001f90a0 + (16 + i) * 0x38) = 0x99;
    }
    *(uint8_t *)ef_guest_memory_ptr(0x001f90a0 + 16 * 0x38) = 0x55;
    assert(clear_panel_strip(17) == 3);
    for (i = 1; i <= 16; ++i) {
        assert(*(uint8_t *)ef_guest_memory_ptr(0x001f90a0 + (16 + i) * 0x38) == 0);
    }
    assert(*(uint8_t *)ef_guest_memory_ptr(0x001f90a0 + 16 * 0x38) == 0x55);
    assert(get16(0x001fc8a6 + 12) == 0);
    assert(get16(0x001fc8a2 + 12) == 0xffff);
    assert(clear_panel_strip(-1) == -3);
    assert(get16(0x001fc8a6 - 12) == 0);
    assert(get16(0x001fc8a2 - 12) == 0xffff);
}

int main(void)
{
    test_clock_helpers();
    test_tactical_lookup();
    test_grid_activity();
    test_ui_panel();
    test_ui_list();
    test_panel_strips();
    return 0;
}
