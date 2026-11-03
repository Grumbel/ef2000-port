/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_OBJECT_HELPERS_H
#define EF_STRUCTS_OBJECT_HELPERS_H

#include "ef_types.h"

/* Partial packed layout. Unobserved regions remain opaque. */
typedef struct EF_PACKED ef_motion_object {
    uint8_t unknown_00[0x14];
    int32_t index_packed;             /* +0x14, signed high word */
    uint8_t unknown_18[0xd1];
    uint8_t class_code;               /* +0xe9 */
    uint8_t unknown_ea[0x8d];
    double current_limit;             /* +0x177 */
} ef_motion_object;
EF_STATIC_ASSERT(ef_motion_object_index_off, offsetof(ef_motion_object, index_packed) == 0x14);
EF_STATIC_ASSERT(ef_motion_object_class_off, offsetof(ef_motion_object, class_code) == 0xe9);
EF_STATIC_ASSERT(ef_motion_object_limit_off, offsetof(ef_motion_object, current_limit) == 0x177);

/* Linked scene records and 0x44-byte child slots. */
typedef struct EF_PACKED ef_scene_node {
    uint8_t unknown_00[0x14];
    int32_t index_packed;             /* +0x14 */
    uint16_t subkind;                 /* +0x18 */
    uint8_t unknown_1a[8];
    uint16_t event_state;             /* +0x22 */
    uint8_t unknown_24[0x0c];
    uint8_t state_bits;               /* +0x30 */
    uint8_t unknown_31[5];
    int32_t owner_packed;             /* +0x36 */
    uint8_t kind;                     /* +0x3a */
    uint8_t unknown_3b[0x0d];
    ef_addr32 child_slots;            /* +0x48 */
    uint8_t unknown_4c[4];
    ef_addr32 next;                   /* +0x50 */
} ef_scene_node;
EF_STATIC_ASSERT(ef_scene_index_off, offsetof(ef_scene_node, index_packed) == 0x14);
EF_STATIC_ASSERT(ef_scene_subkind_off, offsetof(ef_scene_node, subkind) == 0x18);
EF_STATIC_ASSERT(ef_scene_state_off, offsetof(ef_scene_node, event_state) == 0x22);
EF_STATIC_ASSERT(ef_scene_bits_off, offsetof(ef_scene_node, state_bits) == 0x30);
EF_STATIC_ASSERT(ef_scene_owner_off, offsetof(ef_scene_node, owner_packed) == 0x36);
EF_STATIC_ASSERT(ef_scene_kind_off, offsetof(ef_scene_node, kind) == 0x3a);
EF_STATIC_ASSERT(ef_scene_children_off, offsetof(ef_scene_node, child_slots) == 0x48);
EF_STATIC_ASSERT(ef_scene_next_off, offsetof(ef_scene_node, next) == 0x50);

typedef struct EF_PACKED ef_scene_child {
    uint8_t unknown_00[0x16];
    uint8_t flags;                    /* +0x16 */
    uint8_t unknown_17[0x29];
    int32_t index_packed;             /* +0x40 */
} ef_scene_child;
EF_STATIC_ASSERT(ef_scene_child_size, sizeof(ef_scene_child) == 0x44);
EF_STATIC_ASSERT(ef_scene_child_flags_off, offsetof(ef_scene_child, flags) == 0x16);
EF_STATIC_ASSERT(ef_scene_child_index_off, offsetof(ef_scene_child, index_packed) == 0x40);

#endif
