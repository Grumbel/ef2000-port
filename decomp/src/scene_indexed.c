/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x8ffe8, 0x26 bytes, 58 caller functions. The subtraction and signed
 * range check happen only in AX; higher EAX bits never participate. The
 * original shared exit at 0x9000a is reached by other function bodies. */
ef_addr32 lookup_scene_node(int16_t index)
{
    int16_t slot = (int16_t)((uint16_t)index - 0x2001u);
    if (slot < 0 || slot >= g_scene_node_count) {
        return 0;
    }
    const ef_addr32 *table = (const ef_addr32 *)ef_guest_memory_ptr(g_scene_node_table);
    return table[slot];
}

/* 0x75620, 0x56 bytes, four independent caller functions. AL is an
 * unsigned 0..255 percentage, and zero is a distinct category. */
uint32_t classify_object_percentage(uint8_t percentage)
{
    if (percentage == 0) { return 6; }
    if (percentage < 50) { return 4; }
    if (percentage < 75) { return 3; }
    if (percentage <= 99) { return 2; }
    return 1;
}

/* 0x7574c, 0x57 bytes, 19 caller functions. An invalid indexed object
 * never matches, even if the selector/index otherwise equals the key. */
uint8_t object_uses_secondary_grid(int16_t index)
{
    if (lookup_filtered_indexed_object(index) == 0) { return 0; }
    if (index == (int16_t)(g_grid_secondary_selector >> 16)) { return 1; }
    if (g_world_grid_mode != 0) {
        for (unsigned i = 0; i < g_external_grid_count; ++i) {
            int16_t matched = (int16_t)(g_grid_external_probes[i].object_index_packed >> 16);
            if (index == matched) { return 1; }
        }
    }
    return 0;
}

/* 0x75c84, 0x1b bytes, three callers. Preserves EDX in the original. */
void reset_object_link_controls(void)
{
    g_object_control_reset_word = 0;
    g_object_link_reset_fields[1] = 0;
    g_object_link_reset_fields[0] = 0;
}

/* 0x76b30, 0x4f bytes, three callers. The owner number is the parent's
 * byte at +0x187; the table record has a 25-byte stride and slots at
 * +3 + 2*DL, without an original bounds check on DL. */
void attach_object_link_child(ef_addr32 parent, int16_t child_index, uint8_t slot)
{
    const ef_linked_object *owner = (const ef_linked_object *)ef_guest_memory_ptr(parent);
    ef_addr32 slot_address = g_object_link_record_base +
        (ef_addr32)owner->link_owner_slot * 25u + 3u + (ef_addr32)slot * 2u;
    *(int16_t *)ef_guest_memory_ptr(slot_address) = child_index;

    ef_addr32 child_address = lookup_filtered_indexed_object(child_index);
    if (child_address != 0) {
        ef_linked_object *child = (ef_linked_object *)ef_guest_memory_ptr(child_address);
        child->linked_slot = slot;
        child->linked_state = 0;
        child->linked_parent = owner->parent_identifier;
    }
}

/* 0x78aa8, 0x18 bytes, six callers. Callback writes an unknown record at
 * 0x1f8d00; then this wrapper always sets +0x56 to decimal 280. */
void configure_navigation_record(ef_addr32 source)
{
    copy_navigation_record(0x1f8d00, source);
    g_navigation_record.marker = 0x118;
}

/* 0x6ae30, 0x82 bytes, three callers. +0x26 of each indexed object
 * selects a 0xca-byte world-grid row; its +0xb7 high signed word is a
 * linked target. Detail mode 0xb dispatches via a separate original
 * predicate whose *full AX*, not just AL, is checked for nonzero. */
uint8_t indexed_object_has_grid_link(int16_t index)
{
    ef_addr32 object_address = lookup_filtered_indexed_object(index);
    if (object_address == 0) { return 0; }

    const ef_linked_object *object = (const ef_linked_object *)ef_guest_memory_ptr(object_address);
    int16_t grid_index = (int16_t)(object->grid_index_packed >> 16);
    ef_addr32 grid_address = g_world_grid_base + (ef_addr32)(int32_t)grid_index * 0xcau;
    ef_world_grid_cell *grid = (ef_world_grid_cell *)ef_guest_memory_ptr(grid_address);

    if (grid->detail_mode == 11) {
        return (uint8_t)(check_special_object_grid_link(index) != 0);
    }
    int32_t linked_index = grid->row_link_packed >> 16;
    if (linked_index == 255) { return 0; }
    grid->link_pending_marker = 1;
    return dispatch_object_grid_link(index, (int16_t)linked_index);
}

/* 0x68c18, 0x7c bytes, three callers. Reads the *high* signed word
 * at 0x1282e2 + 2*index, i.e. word[index+1] in the overlapping table.
 * Neither index is range-checked. Calls the original pairing remover
 * twice, in the observed order, with EDX=-1 on both calls. */
void clear_paired_object_marks(int16_t index)
{
    int16_t paired = g_object_paired_index_words[(int32_t)index + 1];
    g_object_pair_marks[index].state = 0;
    g_object_pair_marks[paired].state = 0;
    g_object_pair_marks[index].option = 0;
    g_object_pair_marks[paired].option = 0;
    int16_t root = (int16_t)((uint32_t)g_object_pair_root_packed >> 16);
    clear_object_pair_reference(root, index, -1);
    /* The original reloads both the pairing and root from guest globals. */
    paired = g_object_paired_index_words[(int32_t)index + 1];
    root = (int16_t)((uint32_t)g_object_pair_root_packed >> 16);
    clear_object_pair_reference(root, paired, -1);
}
