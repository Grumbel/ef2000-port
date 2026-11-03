/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GLOBALS_H
#define EF_GLOBALS_H

#include "ef_types.h"
#include "ef_structs/hiscore.h"
#include "ef_structs/spatial.h"
#include "ef_structs/world_grid.h"
#include "ef_structs/object_helpers.h"
#include "ef_structs/scene_state.h"
#include "ef_structs/object_links.h"
#include "ef_structs/buffer_state.h"

/* These describe guest addresses, not linked host storage. The patch linker
 * must resolve each symbol to its original address (symbols/globals.tsv).
 * An unresolved struct layout is deliberately represented as raw bytes. */
extern uint8_t g_difficulty;          /* 0x00121e44 */
extern uint32_t __x32_stack_size;     /* 0x00132a28 */
extern uint32_t _dynend;              /* 0x00132a2c */
extern uint32_t _curbrk;              /* 0x00132a30 */
extern ef_addr32 _LpCmdLine;          /* 0x00132a34 */
extern ef_addr32 _LpPgmName;          /* 0x00132a38 */
extern uint16_t _psp;                 /* 0x00132a3c */
extern uint8_t _Envptr[6];           /* 0x00132a61, overlaps _Envseg */
extern uint16_t _Envseg;              /* 0x00132a65 */
extern int32_t g_hiscore_current;     /* 0x001970a0 */
extern ef_hiscore_entry g_hiscore_table[10]; /* 0x001970a4, 10 packed records */
/* Provisional names; original DS locations established from code reads/writes. */
extern ef_spatial_slot g_spatial_slots[29]; /* 0x0019b2e0 */
extern uint8_t g_spatial_slot_tags[29]; /* 0x0019b358..0x0019b374 */
extern int16_t g_spatial_slot_count; /* 0x0019b376, high word of 0x0019b374 */
extern ef_addr32 g_indexed_object_table[]; /* 0x0020f550, 4-byte guest pointers */
extern int16_t g_indexed_object_count; /* 0x002119f4, signed 16-bit bound */
extern uint32_t g_filtered_object_lookup_calls; /* 0x001a3580 */
extern int16_t g_excluded_object_index; /* 0x001f71d0 */
extern uint8_t g_object_type_flags[]; /* 0x00128636, 0xe5-byte stride */
extern ef_addr32 g_world_grid_base; /* 0x0019bb58, pointer to 0xca-byte records */
extern ef_addr32 g_current_object; /* 0x001f71bc, pointer to active object */
extern uint8_t g_world_grid_mode; /* 0x00127da9, gates alternate grid lookup */
extern uint8_t g_world_grid_marker; /* 0x001a666b, compares neighboring rows */
extern uint8_t g_external_grid_count; /* 0x0012583a, number of external rows */
extern int16_t g_active_grid_index; /* 0x0019bb5e, current selected grid row */
extern ef_addr32 g_external_grid_base; /* 0x001a14f8, pointer to 0x45-byte nodes */
extern double g_external_grid_scale; /* 0x00182512, integer-to-world conversion */
extern ef_grid_type_record g_grid_type_records[]; /* 0x001285d5, stride 0xe5 */
extern double g_grid_limit_scale; /* 0x00182605, type-adjusted limit factor */
extern ef_addr32 g_grid_node_tail; /* 0x0019bb38, linked pool queue tail */
extern ef_addr32 g_grid_node_head; /* 0x0019bb3c, linked pool queue head */
extern ef_addr32 g_grid_node_free; /* 0x0019bb40, linked free-node list */
extern ef_addr32 g_grid_adjacency_base; /* 0x001fcb40, 17-byte lists */
extern uint32_t g_grid_secondary_selector; /* 0x001f71c8, signed high index */
extern ef_grid_category_type g_grid_category_types[]; /* 0x0012bc94, stride 0x91 */
extern ef_grid_external_probe g_grid_external_probes[]; /* 0x001a5c2e, stride 0x1b */
extern double g_grid_reference_position[2]; /* 0x001a3020 */
extern double g_grid_visibility_distance; /* 0x001252a4 */
extern double g_grid_distance_scale; /* 0x0018251a, 2D distance conversion */
extern uint8_t g_object_kind_modes[]; /* 0x00128568, stride 0xe5 */
extern uint8_t g_reference_clock[6]; /* 0x0012583c, three packed signed words */
extern ef_addr32 g_scene_head; /* 0x00211ad4, singly linked scene nodes */
extern uint8_t g_scene_kind_child_counts[]; /* 0x00123819, stride 0xa4 */
extern ef_addr32 g_scene_query_pair[2]; /* 0x0021444c, 0x214450 */
extern int32_t g_scene_metric_ref_packed; /* 0x001a35f8 */
extern ef_scene_triplet_packed g_scene_input_triplet; /* 0x000c4756 */
extern ef_scene_triplet_high g_scene_input_high; /* 0x00159094 */
extern int32_t g_scene_runtime_triplet[3]; /* 0x002143ec, Z/Y/X order */
extern int16_t g_scene_last_angles[3]; /* 0x002144a4, AX/DX/BX */
extern ef_scene_event_counter g_scene_event_counters; /* 0x002144aa */
extern int16_t g_scene_event_history[]; /* 0x00214428 */
extern int16_t g_scene_event_armed; /* 0x00214492 */
extern uint8_t g_scene_control_code; /* 0x0012db9e */
extern ef_addr32 g_scene_node_table; /* 0x00211ad0, index table pointer */
extern int16_t g_scene_node_count; /* 0x00211ae0, signed count */
extern int32_t g_object_control_reset_word; /* 0x00128550 */
extern int16_t g_object_link_reset_fields[2]; /* 0x001f88ee, 0x1f88f0 */
extern ef_addr32 g_object_link_record_base; /* 0x001f8918, 25-byte rows */
extern ef_navigation_record g_navigation_record; /* 0x001f8d00 */
extern ef_object_pair_mark g_object_pair_marks[]; /* 0x001f6ef4, 3-byte rows */
extern int16_t g_object_paired_index_words[]; /* 0x001282e2, high word of 32-bit at 2*index */
extern int32_t g_object_pair_root_packed; /* 0x001282da */
extern ef_addr32 g_buffer_start; /* 0x001f8e90 */
extern ef_addr32 g_buffer_cursor; /* 0x001f8e94 */
extern int32_t g_plane_selected_mode; /* 0x0012afc0 */
extern ef_plane_source g_plane_source_table[]; /* 0x001f8e00, stride 12 */
extern ef_addr32 g_active_plane_high; /* 0x001f8eb4 */
extern ef_addr32 g_active_plane_low; /* 0x001f8eb8 */
extern ef_addr32 g_active_plane_aux; /* 0x001f8ea4 */
extern uint8_t g_scene_refresh_pending; /* 0x0012dbc4 */

#endif
