/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_OBJECT_LINKS_H
#define EF_STRUCTS_OBJECT_LINKS_H

#include "ef_types.h"

/* Partial object layout used by 0x6ae30 and 0x76b30. The original object
 * tables have a different allocation lifetime than the short link records. */
typedef struct EF_PACKED ef_linked_object {
    uint8_t unknown_00[0x16];
    int16_t parent_identifier;         /* +0x16 */
    uint8_t unknown_18[0x0e];
    int32_t grid_index_packed;         /* +0x26, high signed word */
    uint8_t unknown_2a[0x15d];
    uint8_t link_owner_slot;           /* +0x187 */
    int16_t linked_parent;             /* +0x188 */
    uint8_t linked_state;              /* +0x18a */
    uint8_t unknown_18b;
    uint8_t linked_slot;               /* +0x18c */
} ef_linked_object;
EF_STATIC_ASSERT(ef_linked_object_id_off, offsetof(ef_linked_object, parent_identifier) == 0x16);
EF_STATIC_ASSERT(ef_linked_object_grid_off, offsetof(ef_linked_object, grid_index_packed) == 0x26);
EF_STATIC_ASSERT(ef_linked_object_owner_off, offsetof(ef_linked_object, link_owner_slot) == 0x187);
EF_STATIC_ASSERT(ef_linked_object_parent_off, offsetof(ef_linked_object, linked_parent) == 0x188);
EF_STATIC_ASSERT(ef_linked_object_state_off, offsetof(ef_linked_object, linked_state) == 0x18a);
EF_STATIC_ASSERT(ef_linked_object_slot_off, offsetof(ef_linked_object, linked_slot) == 0x18c);

/* 0x1f6ef4: three-byte marks indexed by signed object identifiers. */
typedef struct EF_PACKED ef_object_pair_mark {
    uint8_t state;
    uint16_t option;
} ef_object_pair_mark;
EF_STATIC_ASSERT(ef_object_pair_mark_size, sizeof(ef_object_pair_mark) == 3);

/* 0x1f8d00: the source copier at 0x78aa8 writes the marker at +0x56. */
typedef struct EF_PACKED ef_navigation_record {
    uint8_t unknown_00[0x56];
    uint16_t marker;
} ef_navigation_record;
EF_STATIC_ASSERT(ef_navigation_record_marker_off, offsetof(ef_navigation_record, marker) == 0x56);

#endif
