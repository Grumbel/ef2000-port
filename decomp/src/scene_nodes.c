/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static int16_t high_word(int32_t packed)
{
    return (int16_t)((uint32_t)packed >> 16);
}

static ef_scene_node *scene_node(ef_addr32 address)
{
    return (ef_scene_node *)ef_guest_memory_ptr(address);
}

/* 0x00090050, size 0x22. Caller functions 0x90880 and 0x90924 test AL.
 * A linked node's +0x14 index must resolve to the very same pointer. */
uint8_t scene_node_is_registered(ef_addr32 node)
{
    if (node == 0) {
        return 0;
    }
    return (uint8_t)(node == lookup_scene_node(high_word(scene_node(node)->index_packed)));
}

/* 0x00090074, size 0x4a. Callers 0x56890 and 0x90124; mark parent
 * state flags and clear bit 2 of every child slot's +0x16 byte.
 * The slot count comes from a 0xa4-byte kind table, not the list size. */
void clear_scene_node_children(int16_t index)
{
    ef_addr32 address = lookup_scene_node(index);
    if (address != 0) {
        ef_scene_node *node = scene_node(address);
        uint8_t count = g_scene_kind_child_counts[(uint32_t)node->kind * 0xa4u];
        ef_scene_child *children = (ef_scene_child *)ef_guest_memory_ptr(node->child_slots);
        uint16_t i;
        node->state_bits |= 3;
        for (i = 0; i < count; ++i) {
            children[i].flags &= (uint8_t)~4u;
        }
        notify_scene_owner(high_word(node->owner_packed));
    }
}

/* 0x000900c0, size 0x61, independent caller functions 0x566b4 and
 * 0x90438. In the alternate mode, a mismatched +0x18 subkind chooses
 * the external dispatch. Only the direct path sets +0x22 to five. */
void dispatch_scene_node_event(int16_t index, int16_t event, uint8_t code)
{
    ef_addr32 address = lookup_scene_node(index);
    if (address != 0) {
        ef_scene_node *node = scene_node(address);
        if (g_world_grid_mode != 0 && node->subkind != g_world_grid_marker) {
            dispatch_scene_node_external(index, event, code);
        } else {
            dispatch_scene_node_direct(index, event, code);
            node->event_state = 5;
        }
    }
}

/* 0x000901ec, size 0x2e. Caller functions 0x21e04 and 0x3dd30.
 * Deliberately re-read +0x50 *after* the callback: the original allows
 * a callback to alter the next link before traversal continues. */
void visit_all_scene_nodes(void)
{
    ef_addr32 current = g_scene_head;
    while (current != 0) {
        ef_scene_node *node = scene_node(current);
        if (node->kind < 0x1b) {
            visit_scene_node_standard(current);
        } else {
            visit_scene_node_extended(current);
        }
        current = node->next;
    }
}

/* 0x0009065c, size 0x0e, callers 0x62edc, 0x6abac, 0x8be54.
 * NULL requests the first scene node. */
ef_addr32 next_scene_node(ef_addr32 node)
{
    return node == 0 ? g_scene_head : scene_node(node)->next;
}

/* 0x000906b4, size 0x4e. Callers 0x65e10, 0x89698,
 * 0x9075c and 0x907f0. A non-null filtered indexed object in any
 * 0x44-byte child slot suffices; zero count returns false. */
uint8_t scene_node_has_active_children(int16_t index)
{
    ef_addr32 address = lookup_scene_node(index);
    if (address != 0) {
        const ef_scene_node *node = scene_node(address);
        const ef_scene_child *children =
            (const ef_scene_child *)ef_guest_memory_ptr(node->child_slots);
        uint8_t count = g_scene_kind_child_counts[(uint32_t)node->kind * 0xa4u];
        uint16_t i;
        for (i = 0; i < count; ++i) {
            if (lookup_filtered_indexed_object(high_word(children[i].index_packed)) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* 0x000907f0, size 0x35. Caller functions 0x65d24 and 0x65e10.
 * Starts at the successor of a supplied node, or at the list head.
 * The original shares the 0x9000a EAX-return epilogue with 0x8ffe8;
 * any eventual guest splice must include/retarget that out-of-line tail. */
ef_addr32 next_scene_node_with_active_children(ef_addr32 node)
{
    ef_addr32 current = next_scene_node(node);
    while (current != 0) {
        if (scene_node_has_active_children(high_word(scene_node(current)->index_packed))) {
            return current;
        }
        current = scene_node(current)->next;
    }
    return 0;
}
