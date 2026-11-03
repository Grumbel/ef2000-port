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
    uint8_t current_mode;             /* +0x1a */
    uint8_t detail_mode;              /* +0x1b */
    int16_t adjacency_group;          /* +0x1c, selects 17-byte neighbor list */
    uint8_t unknown_1e[2];
    uint8_t reset_state;              /* +0x20 */
    uint8_t unknown_21;
    int16_t reverse_link;             /* +0x22 */
    int16_t forward_link;             /* +0x24 */
    uint8_t link_state;               /* +0x26 */
    double inherited_position_a;     /* +0x27, copied from linked row +0x61 */
    double inherited_position_b;     /* +0x2f, copied from linked row +0x69 */
    double inherited_position_c;     /* +0x37, copied from linked row +0x71 */
    uint8_t unknown_3f[0x0a];
    uint8_t state_enabled;            /* +0x49 */
    uint8_t activation_active;        /* +0x4a */
    uint8_t unknown_4b;
    uint8_t activation_requested;     /* +0x4c */
    uint8_t unknown_4d;
    int32_t indexed_object_packed;    /* +0x4e: signed high word */
    uint8_t unknown_52[0x0f];
    double position_a;               /* +0x61 */
    double position_b;               /* +0x69 */
    double position_c;               /* +0x71 */
    uint8_t unknown_79[0x17];
    uint8_t event_value;              /* +0x90 */
    uint32_t retained_a;              /* +0x91 */
    uint32_t retained_b;              /* +0x95 */
    uint32_t pending_a;               /* +0x99 */
    uint32_t pending_b;               /* +0x9d */
    uint8_t unknown_a1[0x1a];
    uint8_t link_pending_marker;      /* +0xbb, mode-dependent sentinel */
    uint8_t category_code;            /* +0xbc, selected object feature */
    uint8_t unknown_bd[0x0d];
} ef_world_grid_cell;

EF_STATIC_ASSERT(ef_grid_cell_size, sizeof(ef_world_grid_cell) == 0xca);
EF_STATIC_ASSERT(ef_grid_type_off, offsetof(ef_world_grid_cell, object_type) == 0x19);
EF_STATIC_ASSERT(ef_grid_mode_off, offsetof(ef_world_grid_cell, current_mode) == 0x1a);
EF_STATIC_ASSERT(ef_grid_detail_off, offsetof(ef_world_grid_cell, detail_mode) == 0x1b);
EF_STATIC_ASSERT(ef_grid_adjacency_off, offsetof(ef_world_grid_cell, adjacency_group) == 0x1c);
EF_STATIC_ASSERT(ef_grid_locator_off, offsetof(ef_world_grid_cell, locator_packed) == 0x13);
EF_STATIC_ASSERT(ef_grid_reset_off, offsetof(ef_world_grid_cell, reset_state) == 0x20);
EF_STATIC_ASSERT(ef_grid_reverse_off, offsetof(ef_world_grid_cell, reverse_link) == 0x22);
EF_STATIC_ASSERT(ef_grid_forward_off, offsetof(ef_world_grid_cell, forward_link) == 0x24);
EF_STATIC_ASSERT(ef_grid_linkstate_off, offsetof(ef_world_grid_cell, link_state) == 0x26);
EF_STATIC_ASSERT(ef_grid_inherited_a_off, offsetof(ef_world_grid_cell, inherited_position_a) == 0x27);
EF_STATIC_ASSERT(ef_grid_inherited_b_off, offsetof(ef_world_grid_cell, inherited_position_b) == 0x2f);
EF_STATIC_ASSERT(ef_grid_inherited_c_off, offsetof(ef_world_grid_cell, inherited_position_c) == 0x37);
EF_STATIC_ASSERT(ef_grid_state_off, offsetof(ef_world_grid_cell, state_enabled) == 0x49);
EF_STATIC_ASSERT(ef_grid_active_off, offsetof(ef_world_grid_cell, activation_active) == 0x4a);
EF_STATIC_ASSERT(ef_grid_request_off, offsetof(ef_world_grid_cell, activation_requested) == 0x4c);
EF_STATIC_ASSERT(ef_grid_index_off, offsetof(ef_world_grid_cell, indexed_object_packed) == 0x4e);
EF_STATIC_ASSERT(ef_grid_position_a_off, offsetof(ef_world_grid_cell, position_a) == 0x61);
EF_STATIC_ASSERT(ef_grid_position_b_off, offsetof(ef_world_grid_cell, position_b) == 0x69);
EF_STATIC_ASSERT(ef_grid_position_c_off, offsetof(ef_world_grid_cell, position_c) == 0x71);
EF_STATIC_ASSERT(ef_grid_event_off, offsetof(ef_world_grid_cell, event_value) == 0x90);
EF_STATIC_ASSERT(ef_grid_retained_a_off, offsetof(ef_world_grid_cell, retained_a) == 0x91);
EF_STATIC_ASSERT(ef_grid_retained_b_off, offsetof(ef_world_grid_cell, retained_b) == 0x95);
EF_STATIC_ASSERT(ef_grid_pending_a_off, offsetof(ef_world_grid_cell, pending_a) == 0x99);
EF_STATIC_ASSERT(ef_grid_pending_b_off, offsetof(ef_world_grid_cell, pending_b) == 0x9d);
EF_STATIC_ASSERT(ef_grid_link_pending_off, offsetof(ef_world_grid_cell, link_pending_marker) == 0xbb);
EF_STATIC_ASSERT(ef_grid_category_off, offsetof(ef_world_grid_cell, category_code) == 0xbc);

/* 0x1fcb40 points at 17-byte neighbor lists. The zeroth byte is not
 * consulted by the 0x2bbd0 query. Each index is a signed word. */
typedef struct EF_PACKED ef_grid_adjacency {
    uint8_t unknown_00;
    int16_t neighbors[8];
} ef_grid_adjacency;
EF_STATIC_ASSERT(ef_grid_adjacency_size, sizeof(ef_grid_adjacency) == 0x11);
EF_STATIC_ASSERT(ef_grid_adjacency_neighbors_off, offsetof(ef_grid_adjacency, neighbors) == 1);

/* 0x12bc94: only the first byte of each 0x91-byte category description is
 * touched by the original 0x2da54 implementation. */
typedef struct EF_PACKED ef_grid_category_type {
    uint8_t allowed_kind;
    uint8_t unknown_01[0x90];
} ef_grid_category_type;
EF_STATIC_ASSERT(ef_grid_category_type_size, sizeof(ef_grid_category_type) == 0x91);

/* 0x1a5c2e: fixed external probes, 27 bytes each; the point at +5 has
 * two doubles used by the 0x2dbf8 dependency. */
typedef struct EF_PACKED ef_grid_external_probe {
    uint32_t object_index_packed;
    uint8_t unknown_04;
    double position[2];
    uint8_t unknown_15[6];
} ef_grid_external_probe;
EF_STATIC_ASSERT(ef_grid_external_probe_size, sizeof(ef_grid_external_probe) == 0x1b);
EF_STATIC_ASSERT(ef_grid_external_probe_position_off, offsetof(ef_grid_external_probe, position) == 5);

/* Node pool near 0x19bb38: original allocation nodes have a next pointer
 * at +0x12. Only this field is recovered. */
typedef struct EF_PACKED ef_grid_pool_node {
    uint8_t unknown_00[0x12];
    ef_addr32 next;
} ef_grid_pool_node;
EF_STATIC_ASSERT(ef_grid_pool_next_off, offsetof(ef_grid_pool_node, next) == 0x12);

/* Non-grid pointer at 0x1a14f8: 69-byte external-position records. */
typedef struct EF_PACKED ef_external_grid_node {
    uint8_t unknown_00;
    uint8_t back_reference;          /* +0x01, byte-truncated cell index */
    uint8_t unknown_02[5];
    int32_t component_a;             /* +0x07 */
    int32_t component_b;             /* +0x0b */
    int32_t component_c;             /* +0x0f */
    uint8_t unknown_13[0x31];
    uint8_t marker;                  /* +0x44 */
} ef_external_grid_node;
EF_STATIC_ASSERT(ef_external_grid_node_size, sizeof(ef_external_grid_node) == 0x45);
EF_STATIC_ASSERT(ef_external_backref_off, offsetof(ef_external_grid_node, back_reference) == 0x01);
EF_STATIC_ASSERT(ef_external_a_off, offsetof(ef_external_grid_node, component_a) == 0x07);
EF_STATIC_ASSERT(ef_external_b_off, offsetof(ef_external_grid_node, component_b) == 0x0b);
EF_STATIC_ASSERT(ef_external_c_off, offsetof(ef_external_grid_node, component_c) == 0x0f);
EF_STATIC_ASSERT(ef_external_marker_off, offsetof(ef_external_grid_node, marker) == 0x44);

/* 0x1285d5: per-kind 0xe5-byte record, overlapping the known type flags
 * at +0x61. The doubles are an initial limit plus two adjustments. */
typedef struct EF_PACKED ef_grid_type_record {
    double limit_a;
    double limit_b;
    double limit_multiplier;
    double limit_extra;             /* +0x18, fourth limit term */
    uint8_t unknown_20[0xc5];
} ef_grid_type_record;
EF_STATIC_ASSERT(ef_grid_type_record_size, sizeof(ef_grid_type_record) == 0xe5);
EF_STATIC_ASSERT(ef_grid_type_limit_a_off, offsetof(ef_grid_type_record, limit_a) == 0x00);
EF_STATIC_ASSERT(ef_grid_type_limit_b_off, offsetof(ef_grid_type_record, limit_b) == 0x08);
EF_STATIC_ASSERT(ef_grid_type_multiplier_off, offsetof(ef_grid_type_record, limit_multiplier) == 0x10);
EF_STATIC_ASSERT(ef_grid_type_extra_off, offsetof(ef_grid_type_record, limit_extra) == 0x18);

#endif
