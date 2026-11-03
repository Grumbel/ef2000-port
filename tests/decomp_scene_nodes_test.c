/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>

static uint8_t guest[0x20000];
static ef_addr32 indexed[16];
ef_addr32 g_scene_head = 0x1000;
uint8_t g_scene_kind_child_counts[256 * 0xa4];
uint8_t g_world_grid_mode;
uint8_t g_world_grid_marker;
static int direct_calls;
static int external_calls;
static int owner_calls;
static int standard_calls;
static int extended_calls;
static int last_owner;
static int last_event;
static int last_code;
static int mutate_next;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

ef_addr32 lookup_scene_node(int16_t index)
{
    return index >= 0x2001 && index <= 0x2003 ?
        (ef_addr32)(0x1000 + (index - 0x2001) * 0x100) : 0;
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    return index >= 0 && index < 16 ? indexed[index] : 0;
}

void notify_scene_owner(int16_t owner)
{
    ++owner_calls;
    last_owner = owner;
}

void dispatch_scene_node_direct(int16_t index, int16_t event, uint8_t code)
{
    assert(index == 0x2001);
    ++direct_calls;
    last_event = event;
    last_code = code;
}

void dispatch_scene_node_external(int16_t index, int16_t event, uint8_t code)
{
    assert(index == 0x2001);
    ++external_calls;
    last_event = event;
    last_code = code;
}

void visit_scene_node_standard(ef_addr32 node)
{
    ++standard_calls;
    if (mutate_next && node == 0x1000) {
        ((ef_scene_node *)(guest + node))->next = 0x1200;
    }
}

void visit_scene_node_extended(ef_addr32 node)
{
    assert(node == 0x1200);
    ++extended_calls;
}

int main(void)
{
    ef_scene_node *a = (ef_scene_node *)(guest + 0x1000);
    ef_scene_node *b = (ef_scene_node *)(guest + 0x1100);
    ef_scene_node *c = (ef_scene_node *)(guest + 0x1200);
    ef_scene_child *children_a = (ef_scene_child *)(guest + 0x5000);
    ef_scene_child *children_b = (ef_scene_child *)(guest + 0x6000);
    a->index_packed = 0x2001 << 16;
    b->index_packed = 0x2002 << 16;
    c->index_packed = 0x2003 << 16;
    a->owner_packed = 0x2020 << 16;
    a->next = 0x1100;
    b->next = 0x1200;
    a->kind = 2;
    b->kind = 2;
    c->kind = 27;
    a->child_slots = 0x5000;
    b->child_slots = 0x6000;
    g_scene_kind_child_counts[2 * 0xa4] = 2;
    a->subkind = 4;

    assert(scene_node_is_registered(0) == 0);
    assert(scene_node_is_registered(0x1000) == 1);
    a->index_packed = 0x2002 << 16;
    assert(scene_node_is_registered(0x1000) == 0);
    a->index_packed = 0x2001 << 16;

    children_a[0].flags = 0xff;
    children_a[1].flags = 0xfd;
    a->state_bits = 0xf0;
    clear_scene_node_children(0x2001);
    assert(a->state_bits == 0xf3);
    assert(children_a[0].flags == 0xfb);
    assert(children_a[1].flags == 0xf9);
    assert(owner_calls == 1 && last_owner == 0x2020);
    clear_scene_node_children(0x1fff);
    assert(owner_calls == 1);

    dispatch_scene_node_event(0x2001, -7, 0xa5);
    assert(direct_calls == 1 && a->event_state == 5);
    assert(last_event == -7 && last_code == 0xa5);
    a->event_state = 0;
    g_world_grid_mode = 1;
    g_world_grid_marker = 8;
    dispatch_scene_node_event(0x2001, 15, 0x33);
    assert(external_calls == 1 && a->event_state == 0);
    assert(last_event == 15 && last_code == 0x33);
    g_world_grid_marker = 4;
    dispatch_scene_node_event(0x2001, 0, 0);
    assert(direct_calls == 2 && a->event_state == 5);

    assert(next_scene_node(0) == 0x1000);
    assert(next_scene_node(0x1000) == 0x1100);
    assert(next_scene_node(0x1200) == 0);
    assert(scene_node_has_active_children(0x2001) == 0);
    indexed[5] = 0x7000;
    children_b[0].index_packed = 5 << 16;
    assert(scene_node_has_active_children(0x2002) == 1);
    assert(next_scene_node_with_active_children(0) == 0x1100);
    assert(next_scene_node_with_active_children(0x1000) == 0x1100);
    assert(next_scene_node_with_active_children(0x1100) == 0);
    assert(next_scene_node_with_active_children(0x1200) == 0);
    indexed[5] = 0;
    assert(next_scene_node_with_active_children(0) == 0);

    mutate_next = 1;
    visit_all_scene_nodes();
    assert(standard_calls == 1 && extended_calls == 1);
    return 0;
}
