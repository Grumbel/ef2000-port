/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t guest[0x40000];
static ef_addr32 filtered[24];
ef_addr32 g_scene_node_table;
int16_t g_scene_node_count;
uint32_t g_grid_secondary_selector;
uint8_t g_world_grid_mode;
uint8_t g_external_grid_count;
ef_grid_external_probe g_grid_external_probes[4];
int32_t g_object_control_reset_word;
int16_t g_object_link_reset_fields[2];
ef_addr32 g_object_link_record_base;
ef_navigation_record g_navigation_record;
ef_addr32 g_world_grid_base;
ef_object_pair_mark g_object_pair_marks[20];
int16_t g_object_paired_index_words[24];
int32_t g_object_pair_root_packed;

static int navigation_copies;
static ef_addr32 navigation_src;
static int special_calls;
static int16_t special_index;
static int16_t special_return;
static int dispatch_calls;
static int16_t dispatch_index;
static int16_t dispatch_target;
static uint8_t dispatch_return;
static int clear_reference_calls;
static int16_t cleared_roots[2];
static int16_t cleared_others[2];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    return index >= 0 && index < 24 ? filtered[index] : 0;
}

void copy_navigation_record(ef_addr32 target, ef_addr32 source)
{
    assert(target == 0x1f8d00);
    ++navigation_copies;
    navigation_src = source;
    g_navigation_record.marker = 0;
}

int16_t check_special_object_grid_link(int16_t index)
{
    ++special_calls;
    special_index = index;
    return special_return;
}

uint8_t dispatch_object_grid_link(int16_t index, int16_t linked)
{
    ++dispatch_calls;
    dispatch_index = index;
    dispatch_target = linked;
    return dispatch_return;
}

void clear_object_pair_reference(int16_t root, int16_t other, int32_t sentinel)
{
    assert(sentinel == -1);
    assert(clear_reference_calls < 2);
    cleared_roots[clear_reference_calls] = root;
    cleared_others[clear_reference_calls] = other;
    ++clear_reference_calls;
    if (clear_reference_calls == 1) {
        /* The original reloads the pair and root for the second call. */
        g_object_paired_index_words[5] = 8;
        g_object_pair_root_packed = 23 << 16;
    }
}

int main(void)
{
    ef_addr32 *scene = (ef_addr32 *)(guest + 0x100);
    scene[0] = 0x12100;
    scene[1] = 0;
    scene[2] = 0x12300;
    g_scene_node_table = 0x100;
    g_scene_node_count = 3;
    assert(lookup_scene_node(0x2001) == 0x12100);
    assert(lookup_scene_node(0x2002) == 0);
    assert(lookup_scene_node(0x2003) == 0x12300);
    assert(lookup_scene_node(0x2000) == 0);
    assert(lookup_scene_node(0x2004) == 0);
    g_scene_node_count = 0;
    assert(lookup_scene_node(0x2001) == 0);

    for (unsigned value = 0; value <= 255; ++value) {
        uint8_t expected = value == 0 ? 6 : value < 50 ? 4 :
            value < 75 ? 3 : value < 100 ? 2 : 1;
        assert(classify_object_percentage((uint8_t)value) == expected);
    }

    filtered[5] = 0x1000;
    filtered[6] = 0x2000;
    filtered[7] = 0x3000;
    g_grid_secondary_selector = 5 << 16;
    assert(object_uses_secondary_grid(5) == 1);
    assert(object_uses_secondary_grid(4) == 0);
    assert(object_uses_secondary_grid(6) == 0);
    g_world_grid_mode = 1;
    g_external_grid_count = 2;
    g_grid_external_probes[0].object_index_packed = 12 << 16;
    g_grid_external_probes[1].object_index_packed = 6 << 16;
    assert(object_uses_secondary_grid(6) == 1);
    assert(object_uses_secondary_grid(7) == 0);
    assert(object_uses_secondary_grid(12) == 0);
    g_world_grid_mode = 0;
    assert(object_uses_secondary_grid(6) == 0);

    g_object_control_reset_word = 0x12345678;
    g_object_link_reset_fields[0] = 12;
    g_object_link_reset_fields[1] = -34;
    reset_object_link_controls();
    assert(g_object_control_reset_word == 0);
    assert(g_object_link_reset_fields[0] == 0 && g_object_link_reset_fields[1] == 0);

    ef_linked_object *parent = (ef_linked_object *)(guest + 0x1000);
    ef_linked_object *child = (ef_linked_object *)(guest + 0x2000);
    parent->link_owner_slot = 4;
    parent->parent_identifier = -27;
    g_object_link_record_base = 0x9000;
    child->linked_slot = 0;
    child->linked_state = 123;
    child->linked_parent = 0;
    attach_object_link_child(0x1000, 6, 2);
    assert(*(int16_t *)(guest + 0x9000 + 4 * 25 + 3 + 2 * 2) == 6);
    assert(child->linked_slot == 2 && child->linked_state == 0);
    assert(child->linked_parent == -27);
    attach_object_link_child(0x1000, 11, 3);
    assert(*(int16_t *)(guest + 0x9000 + 4 * 25 + 3 + 2 * 3) == 11);

    configure_navigation_record(0xabcde);
    assert(navigation_copies == 1 && navigation_src == 0xabcde);
    assert(g_navigation_record.marker == 0x118);

    g_world_grid_base = 0x5000;
    child->grid_index_packed = 2 << 16;
    ef_world_grid_cell *cell = (ef_world_grid_cell *)(guest + 0x5000 + 2 * 0xca);
    cell->detail_mode = 11;
    special_return = 0x100;
    assert(indexed_object_has_grid_link(6) == 1);
    assert(special_calls == 1 && special_index == 6);
    special_return = 0;
    assert(indexed_object_has_grid_link(6) == 0);
    assert(indexed_object_has_grid_link(11) == 0);
    cell->detail_mode = 4;
    cell->row_link_packed = 255 << 16;
    assert(indexed_object_has_grid_link(6) == 0 && dispatch_calls == 0);
    cell->row_link_packed = -7 * 65536;
    dispatch_return = 0x42;
    assert(indexed_object_has_grid_link(6) == 0x42);
    assert(cell->link_pending_marker == 1);
    assert(dispatch_calls == 1 && dispatch_index == 6 && dispatch_target == -7);

    for (unsigned i = 0; i < 20; ++i) {
        g_object_pair_marks[i].state = 0x77;
        g_object_pair_marks[i].option = 0x8888;
    }
    g_object_paired_index_words[5] = 7;
    g_object_pair_root_packed = 9 << 16;
    clear_paired_object_marks(4);
    assert(g_object_pair_marks[4].state == 0 && g_object_pair_marks[4].option == 0);
    assert(g_object_pair_marks[7].state == 0 && g_object_pair_marks[7].option == 0);
    assert(g_object_pair_marks[8].state == 0x77 && g_object_pair_marks[8].option == 0x8888);
    assert(clear_reference_calls == 2);
    assert(cleared_roots[0] == 9 && cleared_others[0] == 4);
    assert(cleared_roots[1] == 23 && cleared_others[1] == 8);
    return 0;
}
