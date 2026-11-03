/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WORLD_GRID_H
#define EF_STRUCTS_WORLD_GRID_H

#include "ef_types.h"

/* Only observed offsets have names. The four bytes at +0x4e are a packed
 * field whose high signed word identifies the indexed object. */
typedef struct EF_PACKED ef_world_grid_cell {
    uint8_t unknown_00[0x19];
    uint8_t object_type;              /* +0x19 */
    uint8_t unknown_1a[0x2f];
    uint8_t state_enabled;            /* +0x49 */
    uint8_t activation_active;        /* +0x4a */
    uint8_t unknown_4b;
    uint8_t activation_requested;     /* +0x4c */
    uint8_t unknown_4d;
    int32_t indexed_object_packed;    /* +0x4e: signed high word */
    uint8_t unknown_52[0x3e];
    uint8_t event_value;              /* +0x90 */
    uint8_t unknown_91[0x39];
} ef_world_grid_cell;

EF_STATIC_ASSERT(ef_grid_cell_size, sizeof(ef_world_grid_cell) == 0xca);
EF_STATIC_ASSERT(ef_grid_type_off, offsetof(ef_world_grid_cell, object_type) == 0x19);
EF_STATIC_ASSERT(ef_grid_state_off, offsetof(ef_world_grid_cell, state_enabled) == 0x49);
EF_STATIC_ASSERT(ef_grid_active_off, offsetof(ef_world_grid_cell, activation_active) == 0x4a);
EF_STATIC_ASSERT(ef_grid_request_off, offsetof(ef_world_grid_cell, activation_requested) == 0x4c);
EF_STATIC_ASSERT(ef_grid_index_off, offsetof(ef_world_grid_cell, indexed_object_packed) == 0x4e);
EF_STATIC_ASSERT(ef_grid_event_off, offsetof(ef_world_grid_cell, event_value) == 0x90);

#endif
