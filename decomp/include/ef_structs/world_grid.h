/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WORLD_GRID_H
#define EF_STRUCTS_WORLD_GRID_H

#include "ef_types.h"

/* Only observed offsets have names. The four bytes at +0x4e are a packed
 * field whose high signed word identifies the indexed object. */
typedef struct EF_PACKED ef_world_grid_cell {
    uint8_t unknown_00[0x13];
    int32_t locator_packed;          /* +0x13, signed high word */
    uint8_t unknown_17[2];
    uint8_t object_type;              /* +0x19 */
    uint8_t unknown_1a[6];
    uint8_t reset_state;              /* +0x20 */
    uint8_t unknown_21;
    int16_t reverse_link;             /* +0x22 */
    int16_t forward_link;             /* +0x24 */
    uint8_t link_state;               /* +0x26 */
    uint8_t unknown_27[0x22];
    uint8_t state_enabled;            /* +0x49 */
    uint8_t activation_active;        /* +0x4a */
    uint8_t unknown_4b;
    uint8_t activation_requested;     /* +0x4c */
    uint8_t unknown_4d;
    int32_t indexed_object_packed;    /* +0x4e: signed high word */
    uint8_t unknown_52[0x3e];
    uint8_t event_value;              /* +0x90 */
    uint32_t retained_a;              /* +0x91 */
    uint32_t retained_b;              /* +0x95 */
    uint32_t pending_a;               /* +0x99 */
    uint32_t pending_b;               /* +0x9d */
    uint8_t unknown_a1[0x29];
} ef_world_grid_cell;

EF_STATIC_ASSERT(ef_grid_cell_size, sizeof(ef_world_grid_cell) == 0xca);
EF_STATIC_ASSERT(ef_grid_type_off, offsetof(ef_world_grid_cell, object_type) == 0x19);
EF_STATIC_ASSERT(ef_grid_locator_off, offsetof(ef_world_grid_cell, locator_packed) == 0x13);
EF_STATIC_ASSERT(ef_grid_reset_off, offsetof(ef_world_grid_cell, reset_state) == 0x20);
EF_STATIC_ASSERT(ef_grid_reverse_off, offsetof(ef_world_grid_cell, reverse_link) == 0x22);
EF_STATIC_ASSERT(ef_grid_forward_off, offsetof(ef_world_grid_cell, forward_link) == 0x24);
EF_STATIC_ASSERT(ef_grid_linkstate_off, offsetof(ef_world_grid_cell, link_state) == 0x26);
EF_STATIC_ASSERT(ef_grid_state_off, offsetof(ef_world_grid_cell, state_enabled) == 0x49);
EF_STATIC_ASSERT(ef_grid_active_off, offsetof(ef_world_grid_cell, activation_active) == 0x4a);
EF_STATIC_ASSERT(ef_grid_request_off, offsetof(ef_world_grid_cell, activation_requested) == 0x4c);
EF_STATIC_ASSERT(ef_grid_index_off, offsetof(ef_world_grid_cell, indexed_object_packed) == 0x4e);
EF_STATIC_ASSERT(ef_grid_event_off, offsetof(ef_world_grid_cell, event_value) == 0x90);
EF_STATIC_ASSERT(ef_grid_retained_a_off, offsetof(ef_world_grid_cell, retained_a) == 0x91);
EF_STATIC_ASSERT(ef_grid_retained_b_off, offsetof(ef_world_grid_cell, retained_b) == 0x95);
EF_STATIC_ASSERT(ef_grid_pending_a_off, offsetof(ef_world_grid_cell, pending_a) == 0x99);
EF_STATIC_ASSERT(ef_grid_pending_b_off, offsetof(ef_world_grid_cell, pending_b) == 0x9d);

#endif
