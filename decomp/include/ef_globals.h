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
#include "ef_structs/link_resolution.h"
#include "ef_structs/tactical_records.h"

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
extern int16_t g_render_feature_index; /* 0x00198442 */
extern int16_t g_render_feature_limit; /* 0x0019842e */
extern int16_t g_render_feature_enabled; /* 0x00198444 */
extern int16_t g_scene_gate_enabled; /* 0x00425bc8 */
extern int32_t g_scene_gate_target_packed; /* 0x00425bce, signed high word */
extern int32_t g_scene_gate_limit; /* 0x00214448 */
extern uint16_t g_scene_angle_saved_first; /* 0x002144b6 */
extern uint16_t g_scene_angle_saved_second; /* 0x002144b4 */
extern uint16_t g_scene_angle_saved_third; /* 0x002144b0 */
extern double g_scene_vector_scale_a; /* 0x0018a468 */
extern double g_scene_vector_scale_b; /* 0x0018a470 */
extern double g_scene_scalar_scale; /* 0x0018a498 */
extern double g_navigation_vector_scale_a; /* 0x0018aa0b, unaligned */
extern double g_navigation_vector_scale_b; /* 0x0018aa13, unaligned */

/* 0x3856c/0x38658 link resolution and short state-control cluster. */
extern ef_addr32 g_link_pointer_table[]; /* 0x0019f5b0, raw 4-byte slots */
extern uint32_t g_runtime_control_word; /* 0x001a35bc */
extern int16_t g_runtime_phase_word; /* 0x001a35ec */
extern int16_t g_mode_limit_first; /* 0x001a6ce2 */
extern int16_t g_mode_limit_second; /* 0x001a6ce8 */
extern int16_t g_unit_counter; /* 0x0012814e */
extern uint32_t g_buffer_pointer_source; /* 0x0012afc4 */
extern uint32_t g_buffer_pointer_copy; /* 0x001f8e9c */
extern uint8_t g_mode_input_enabled_a; /* 0x0015bac6 */
extern uint8_t g_mode_input_enabled_b; /* 0x0015bac7 */
extern uint8_t g_mode_output_a; /* 0x0012b070 */
extern uint8_t g_mode_output_b; /* 0x0012b104 */
extern uint8_t g_mode_output_c; /* 0x0012b105 */

/* Short tactical-control, clock and coordinate helpers, 0x40820..0x5f1ec. */
extern uint32_t g_tactical_accumulators[3]; /* 0x001a5240 */
extern uint16_t g_tactical_status_words[2]; /* 0x001a527a */
extern uint8_t g_tactical_status_byte; /* 0x001a527e */
extern uint32_t g_tactical_runtime_count; /* 0x00127b20 */
extern uint8_t g_tactical_slots[16]; /* 0x001a5544 */
extern uint8_t g_tactical_reset_mode; /* 0x001a5532 */
extern uint8_t g_tactical_low_flag; /* 0x001a5598 */
extern uint8_t g_tactical_status_entries[25]; /* 0x001a5672, indices 1..24 */
extern uint8_t g_tactical_status_changed; /* 0x001a5708 */
extern uint8_t g_external_grid_marked_count; /* 0x00127da4 */
extern int16_t g_tactical_min_x; /* 0x001a6cf2 */
extern int16_t g_tactical_max_x; /* 0x001a6cf8 */
extern int16_t g_tactical_min_y; /* 0x001a6cf0 */
extern int16_t g_tactical_max_y; /* 0x001a6cf6 */
extern ef_addr32 g_active_coordinate_source; /* 0x001a6b08 */

/* Grid/list/query helpers, 0x40774 and 0x809bc..0x8d0e0. */
extern int16_t g_tactical_lookup_count; /* 0x001357b0 */
extern uint8_t g_tactical_lookup_codes[]; /* 0x001a5510 */
extern uint16_t g_ui_panel_defaults[5]; /* 0x0020f182 */
extern uint16_t g_ui_panel_zero_a[2]; /* 0x0020f18e */
extern uint16_t g_ui_panel_zero_b[2]; /* 0x0020f19a */
extern ef_addr32 g_ui_linked_list_head; /* 0x0020f160 */
extern ef_addr32 g_ui_linked_list_sentinel; /* 0x0020f164 */

/* Parser, tactical event and activity-array helpers. */
extern uint8_t g_reference_copy_mode; /* 0x00125849 */
extern uint16_t g_reference_source_word; /* 0x0019bbf0 */
extern uint8_t g_reference_source_a[8]; /* 0x0019bbf2 */
extern uint8_t g_reference_source_b[8]; /* 0x0019bbfa */
extern uint8_t g_reference_mirror[4]; /* 0x00125835, word + two bytes */
extern ef_tactical_ring_entry g_tactical_ring[20]; /* 0x001a4a0c */
extern int16_t g_tactical_ring_cursor; /* 0x001a4b9a */
extern ef_addr32 g_text_scan_cursor; /* 0x001a58ac */
extern uint8_t g_text_line_buffer[]; /* 0x001a5986 */
extern uint16_t g_text_line_column; /* 0x00127ce2 */
extern uint8_t g_text_line_count; /* 0x00127ce4 */
extern uint16_t g_active_unit_record_count; /* 0x001a81c2, zero-tested 16-bit count */
extern ef_unit_scan_record g_active_unit_records[]; /* 0x001a7d33 */
extern uint16_t g_round_robin_cursor; /* 0x001282ac */
extern int16_t g_round_robin_count; /* 0x001a81b0 */
extern uint16_t g_round_robin_ids[]; /* 0x001a78e0 */
extern uint8_t g_reset_flag_a; /* 0x004338d9 */
extern uint8_t g_reset_flag_b; /* 0x00433281 */
extern uint8_t g_reset_flag_c; /* 0x004338da */
extern uint8_t g_reset_flag_d; /* 0x0043328a */
extern uint32_t g_reset_accumulator_a[2]; /* 0x00433861 */
extern uint32_t g_reset_accumulator_b[2]; /* 0x0043328b */
extern uint32_t g_reset_accumulator_c[2]; /* 0x00433919 */

/* Callers of existing clock/object/strip helpers and coordinate transforms. */
extern float g_short_to_float_scale; /* 0x00185b50 */
extern uint16_t g_panel_sequence_cursor; /* 0x001fc96c */
extern ef_addr32 g_last_transform_child; /* 0x0020f148 */
extern double g_transform_scale_a; /* 0x0018a478 */
extern double g_transform_scale_b; /* 0x0018a480 */
extern double g_transform_scale_c; /* 0x0018a488 */
extern double g_transform_scale_d; /* 0x0018a490 */

/* UI control table and release-button bookkeeping. */
extern int32_t g_ui_control_limit; /* 0x0012b0b4 */
extern ef_addr32 g_ui_control_table; /* 0x001f9078 */
extern uint8_t g_ui_button_code_table[10]; /* 0x001f9088 */
extern uint8_t g_ui_control_initialized; /* 0x001f9095 */
extern ef_addr32 g_ui_panel_config_ptr; /* 0x001f9020 */
extern uint8_t g_ui_alternate_message; /* 0x001a53dc */

/* Packed message command and status-output operands. */
extern uint16_t g_packet_2b_word; /* 0x00197c28 */
extern uint8_t g_packet_2b_byte; /* 0x00197c2e */
extern uint8_t g_ui_status_flag_a; /* 0x00420e10 */
extern uint8_t g_ui_status_flag_b; /* 0x00420a71 */
extern uint8_t g_ui_status_flag_c; /* 0x00420a72 */
extern float g_ui_status_scalar; /* 0x0015babc */
extern uint8_t g_tactical_display_mode; /* 0x0012b369 */
extern uint8_t g_tactical_indicator_source; /* 0x001a5c0b */
extern uint8_t g_tactical_indicator_latch; /* 0x00127d9d */
extern int16_t g_tactical_coordinate_offset_y; /* 0x001a6cfa */
extern int16_t g_tactical_coordinate_offset_x; /* 0x001a6cfc */
extern uint8_t g_tactical_marker_code; /* 0x00128148 */
extern uint32_t g_position_bin_packed; /* 0x001a823a, high word rewritten */
extern uint8_t g_tactical_action_enabled; /* 0x00125847 */
extern ef_addr32 g_ui_record_head; /* 0x0020f158 */
extern ef_addr32 g_ui_record_anchor; /* 0x0020f160 */
extern ef_addr32 g_ui_record_end; /* 0x0020f164 */
extern ef_addr32 g_ui_record_cursor; /* 0x0020f168 */
extern uint16_t g_ui_record_pool_count; /* 0x002119f4 */
extern uint32_t g_ui_record_pool_pointer; /* 0x002119e8 */
extern uint16_t g_tactical_cross_radius; /* 0x001a6ce2 */
extern uint32_t g_ui_composite_arg_d; /* 0x001fcc2c */
extern uint32_t g_ui_composite_arg_b; /* 0x001fcc30 */
extern uint32_t g_ui_composite_arg_c; /* 0x001fcc34 */
extern uint32_t g_ui_reference_x; /* 0x001fcc14 */
extern uint32_t g_ui_reference_y; /* 0x001fcc1c */
extern uint16_t g_ui_composite_offset_x; /* 0x001fcc44 */
extern uint16_t g_ui_composite_offset_y; /* 0x001fcc46 */
extern uint16_t g_ui_component_enabled; /* 0x0012b4a6 */
extern uint8_t g_ui_component_latch; /* 0x001fcc43 */
extern uint8_t g_ui_active_record_scan_latch; /* 0x0012d7b3 */

#endif
