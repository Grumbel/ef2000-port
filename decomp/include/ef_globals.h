/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GLOBALS_H
#define EF_GLOBALS_H

#include "ef_types.h"

extern uint8_t g_character_class_table[256]; /* 0x195af0, byte-wrapped c+1 */
extern char g_network_session_title[]; /* 0x19f566, only minimum extent known */
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
extern ef_addr32 g_gui_error_hook;   /* 0x00132a74, indirect error callback */
extern uint32_t __x32_stack_size;     /* 0x00132a28 */
extern uint32_t _dynend;              /* 0x00132a2c */
extern uint32_t _curbrk;              /* 0x00132a30 */
extern ef_addr32 _LpCmdLine;          /* 0x00132a34 */
extern ef_addr32 _LpPgmName;          /* 0x00132a38 */
#ifdef __WATCOMC__
#define _psp ef_game_psp /* avoid clash with <stdlib.h> _psp */
/* Exact typed scalar globals used by misc_drafts1.c. */
extern uint32_t g_parser_error_context; /* 0x00121eac */
extern uint8_t g_runtime_input_byte_c; /* 0x00121e8c */
extern uint32_t g_runtime_current_tick; /* 0x00197c14 */
extern uint8_t g_runtime_coordinate_latch; /* 0x00121ec6 */
extern uint16_t g_runtime_slot_generation; /* 0x0012288a */
extern uint8_t g_marker_selection_value; /* 0x0012583b */
extern uint16_t g_scene_latch_b; /* 0x00127dae */
extern uint32_t g_texture_right_u; /* 0x00485fd8 */
extern uint32_t g_texture_right_v; /* 0x00485fdc */
extern uint32_t g_texture_left_u; /* 0x00485ff0 */
extern uint32_t g_texture_left_v; /* 0x00485ff4 */
extern uint32_t g_renderer_fixed_texture_mode; /* 0x00158f0c */
extern uint32_t g_texture_atlas_page_index; /* 0x00160000 */
extern uint32_t g_current_render_object_flags; /* 0x00420974 */
extern uint32_t g_current_render_object_clip_mask; /* 0x0042099c */
extern uint32_t g_current_render_vertex_count; /* 0x004209d0 */
extern uint8_t g_menu_presentation_disabled; /* 0x001371ee */
extern uint32_t g_data_playback_file_context; /* 0x00160644 */
extern uint32_t g_data_playback_rewind_position; /* 0x00484150 */
extern uint32_t g_data_playback_last_tick; /* 0x00484154 */
extern uint16_t g_data_playback_tick_interval; /* 0x00484466 */
extern uint16_t g_data_playback_pass; /* 0x00484468 */
extern uint8_t g_menu_animation_panel_active; /* 0x0012b0f4 */
extern uint8_t g_script_condition_automatic; /* 0x001371f9 */
extern uint16_t g_registered_callback_count; /* 0x0041eecc */
extern uint32_t g_script_record_repeat; /* 0x001600b8 */
extern uint32_t g_script_record_lookup_result; /* 0x001600c0 */
extern uint16_t g_page_record_rotation_bias; /* 0x00417222 */
extern uint16_t g_script_queue_slot; /* 0x00146e52 */
extern uint8_t g_page_value_updates_enabled; /* 0x00137a00 */
extern uint8_t g_device_event_mode; /* 0x001357c8 */
extern uint8_t g_device_event_scan_enabled; /* 0x0013583c */
extern uint8_t g_comms_initialized; /* 0x0013578c */
extern uint8_t g_comms_initialize_status; /* 0x0013578e */
extern uint16_t g_comms_initial_clock_dx; /* 0x001357d6 */
extern uint32_t g_comms_initial_tick; /* 0x00136dd4 */
extern uint8_t g_comms_initialize_flag_2; /* 0x0013581e */
extern uint32_t g_page_load_remaining; /* 0x00417160 */
extern uint8_t g_parser_state_1371fb; /* 0x001371fb */
extern uint8_t g_parser_state_1371ed; /* 0x001371ed */
extern uint8_t g_parser_state_1371ff; /* 0x001371ff */
extern uint8_t g_parser_state_137200; /* 0x00137200 */
extern uint32_t g_small_glyph_stride; /* 0x001631f4 */
extern uint16_t g_default_entry_override; /* 0x0041eede */
extern uint32_t g_ui_reset_state; /* 0x0041eeac */
extern uint8_t g_ui_reset_flag; /* 0x0041f2e7 */
extern uint8_t g_ui_state_flag_41f833; /* 0x0041f833 */
extern uint8_t g_ui_state_flag_41f2e6; /* 0x0041f2e6 */
extern uint16_t g_ui_state_word_41eed8; /* 0x0041eed8 */
extern uint8_t g_ui_pointer_device_enabled; /* 0x0041f2e1 */
extern uint8_t g_ui_state_byte_13725d; /* 0x0013725d */
extern uint8_t g_ui_state_byte_13725c; /* 0x0013725c */
extern uint8_t g_ui_state_byte_41f2e5; /* 0x0041f2e5 */
extern uint8_t g_ui_state_byte_41f2e4; /* 0x0041f2e4 */
extern uint8_t g_ui_state_byte_41f2de; /* 0x0041f2de */
extern uint16_t g_named_entry_enabled; /* 0x0041eeda */
extern uint8_t g_sound_slot_mode; /* 0x00420a70 */
extern uint8_t g_active_sound_slot; /* 0x0012dd0d */
extern uint8_t g_sound_slot_selected; /* 0x0012dd0c */
extern uint8_t g_comms_device_ready; /* 0x00135798 */
extern uint16_t g_parser_group_count; /* 0x004194c2 */
extern uint32_t g_default_file_translation_mode; /* 0x0016062d */
extern uint32_t g_record_pool_active_count; /* 0x001379b0 */
extern uint8_t g_frontend_connection_mode; /* 0x00125846 */
extern uint8_t g_frontend_network_active; /* 0x00126576 */
extern uint8_t g_frontend_pending_latch; /* 0x0041f2e8 */
extern uint8_t g_assignment_entry_count; /* 0x001a5531 */
extern uint32_t g_file_creation_permission_mask; /* 0x0017d5bc */
extern uint8_t g_session_activity_flag; /* 0x0019f5ae */
extern uint8_t g_session_first_handle_owned; /* 0x0019f59e */
extern uint8_t g_session_second_handle_owned; /* 0x0019f5a3 */
extern uint8_t g_session_state_byte; /* 0x0019f59b */
extern uint32_t g_session_first_handle; /* 0x0019f35c */
extern uint32_t g_session_second_handle; /* 0x0019f360 */
extern uint8_t g_serial_port_index; /* 0x0015f07f */
extern uint8_t g_serial_command_pending; /* 0x0015f694 */
extern uint8_t g_network_driver_mode; /* 0x0013579a */
extern uint8_t g_network_service_clear_gate; /* 0x00125902 */
extern ef_addr32 g_difficulty_row_pool; /* 0x00121e40 */
extern ef_addr32 g_allocation_callback; /* 0x0012dd9c */
extern ef_addr32 g_resource_size_hook; /* 0x00132ab4 */
extern ef_addr32 g_resource_load_hook; /* 0x00132ab8 */
extern ef_addr32 g_page_point_request_hook; /* 0x00132ad4 */
extern ef_addr32 g_positioned_record_hook; /* 0x00132ad8 */
extern ef_addr32 g_page_point_transform_hook; /* 0x00132af0 */
extern ef_addr32 g_page_point_setup_hook; /* 0x00132af4 */
extern ef_addr32 g_quoted_string_table; /* 0x00132b08 */
extern ef_addr32 g_quoted_string_storage; /* 0x00132b0c */
extern ef_addr32 g_script_record_lookup_hook; /* 0x00132b14 */
extern ef_addr32 g_comms_status_message; /* 0x00135790 */
extern ef_addr32 g_device_event_ids; /* 0x00136b84 */
extern ef_addr32 g_device_event_count_pointer; /* 0x00136b88 */
extern ef_addr32 g_active_record_pool; /* 0x001379bc */
extern ef_addr32 g_parser_active_buffer; /* 0x001379c4 */
extern ef_addr32 g_texture_shade_page; /* 0x00159084 */
extern ef_addr32 g_clock_callback; /* 0x0015b620 */
extern ef_addr32 g_video_present_callback; /* 0x0015b74c */
extern ef_addr32 g_video_prepare_callback; /* 0x0015b750 */
extern ef_addr32 g_raster_framebuffer; /* 0x0015b754 */
extern ef_addr32 g_serial_command_text; /* 0x0015f690 */
extern ef_addr32 g_script_record_parameters; /* 0x001600ac */
extern ef_addr32 g_script_record_world_position; /* 0x001600b0 */
extern ef_addr32 g_script_record_rotation; /* 0x001600b4 */
extern ef_addr32 g_renderer_vertex_base; /* 0x0017bf70 */
extern ef_addr32 g_slot_pointer_table; /* 0x001a1860 */
extern ef_addr32 g_secondary_parser_text; /* 0x001a5904 */
extern ef_addr32 g_active_page_record_data; /* 0x00417174 */
extern ef_addr32 g_active_page_point_data; /* 0x0041717c */
extern ef_addr32 g_menu_picture_path_buffer; /* 0x00417254 */
extern ef_addr32 g_menu_animation_interval_buffer; /* 0x00417258 */
extern ef_addr32 g_menu_animation_effect_buffer; /* 0x00417260 */
extern ef_addr32 g_scrolling_text_record; /* 0x004191d0 */
extern ef_addr32 g_parser_group_head; /* 0x00419200 */
extern ef_addr32 g_ui_style_bitmap_source; /* 0x0041eea8 */
extern ef_addr32 g_current_render_object_prepare; /* 0x00420990 */
extern ef_addr32 g_current_render_object_span; /* 0x00420994 */
extern ef_addr32 g_current_render_object_vertex_list; /* 0x004209bc */
extern ef_addr32 g_data_playback_screen_buffer; /* 0x0048415c */

#endif
extern uint16_t _psp;                 /* 0x00132a3c */
extern uint8_t _Envptr[6];           /* 0x00132a61, overlaps _Envseg */
extern uint16_t _Envseg;              /* 0x00132a65 */
extern int32_t g_hiscore_current;     /* 0x001970a0 */
extern ef_hiscore_entry g_hiscore_table[10]; /* 0x001970a4, 10 packed records */
extern ef_hiscore_entry g_default_hiscores[10]; /* 0x121198, only the scores are used */
extern char g_hiscore_name_pool[48][20]; /* 0x121292, names for generated defaults */
/* Provisional names; original DS locations established from code reads/writes. */
extern ef_spatial_slot g_spatial_slots[29]; /* 0x0019b2e0 */
extern uint8_t g_spatial_slot_tags[29]; /* 0x0019b358..0x0019b374 */
extern int16_t g_spatial_slot_count; /* 0x0019b376, high word of 0x0019b374 */
extern ef_addr32 g_indexed_object_table[]; /* 0x0020f550, 4-byte guest pointers */
extern int16_t g_indexed_object_count; /* 0x002119f4, signed 16-bit bound */
extern ef_addr32 g_renderer_frame_callback; /* 0x000fc8a8, indirect frame callback */
extern uint32_t g_filtered_object_lookup_calls; /* 0x001a3580 */
extern int16_t g_excluded_object_index; /* 0x001f71d0 */
extern uint8_t g_object_type_flags[]; /* 0x00128636, 0xe5-byte stride */
extern double g_grid_cell_step_rate; /* 0x1a3140, rate of the current 0x2e740 step */
extern ef_addr32 g_world_grid_base; /* 0x0019bb58, pointer to 0xca-byte records */
extern ef_addr32 g_current_object; /* 0x001f71bc, pointer to active object */
extern uint16_t g_simulation_tick_step; /* 0x00127d92 */
extern uint8_t g_combat_skill_level; /* 0x001264e2 */
extern uint32_t g_video_width; /* 0x0015b744 */
extern ef_addr32 g_blit_src_row; /* 0x0015b738, packed blit source row */
extern ef_addr32 g_blit_dst_row; /* 0x0015b73c, packed blit destination row */
extern uint32_t g_video_height; /* 0x0015b748 */
extern uint32_t g_view_origin_x; /* 0x15bdb8, word stored by 0xdc21c, read as a dword */
extern uint32_t g_view_origin_y; /* 0x15bdbc */
extern uint32_t g_view_clip_left; /* 0x0015bdc0, inclusive bounds */
extern uint32_t g_view_clip_right; /* 0x0015bdc4 */
extern uint32_t g_view_clip_top; /* 0x0015bdc8 */
extern uint32_t g_view_clip_bottom; /* 0x0015bdcc */
extern uint32_t g_view_latched_width; /* 0x0015b76c */
extern uint32_t g_view_latched_height; /* 0x0015b770 */
extern uint8_t g_world_grid_mode; /* 0x00127da9, gates alternate grid lookup */
extern uint8_t g_world_grid_marker; /* 0x001a666b, compares neighboring rows */
extern int16_t g_menu_ring_points[0x8c]; /* 0x1a6670, x/y pairs at the ring radius */
extern int16_t g_menu_ring_inner[0x8c]; /* 0x1a6788, x/y pairs at radius - width */
extern int16_t g_menu_ring_outer[0x8c]; /* 0x1a68a0, x/y pairs at radius + width */
extern uint16_t g_menu_ring_style; /* 0x1a6cc8 */
extern uint16_t g_menu_ring_cursor; /* 0x1a6cd0 */
extern uint16_t g_menu_ring_count; /* 0x1a6cd2 */
extern uint16_t g_menu_ring_shown; /* 0x1a6cd4 */
extern uint8_t g_external_grid_count; /* 0x0012583a, number of external rows */
extern int16_t g_active_grid_index; /* 0x0019bb5e, current selected grid row */
extern ef_addr32 g_external_grid_base; /* 0x001a14f8, pointer to 0x45-byte nodes */
extern int32_t g_follower_clock; /* 0x001a14f0, reference time in seconds */
extern int16_t g_grid_event_pending; /* 0x00127dc4, set when 0x73210 succeeds, cleared by the grid unit passes */
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
extern uint8_t g_reference_clock[8]; /* 0x0012583c, hours/minutes/seconds/ticks */
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
extern uint32_t g_runtime_segment_base; /* 0x00120cc6 */
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
extern uint32_t g_cursor_ray_to_x; /* 0x001fcc2c */
extern uint32_t g_cursor_ray_to_y; /* 0x001fcc30 */
extern uint32_t g_cursor_ray_to_z; /* 0x001fcc34 */
extern uint32_t g_cursor_ray_from_x; /* 0x001fcc14 */
extern uint32_t g_cursor_ray_from_z; /* 0x001fcc1c */
extern uint16_t g_ui_composite_offset_x; /* 0x001fcc44 */
extern uint16_t g_ui_composite_offset_y; /* 0x001fcc46 */
extern uint16_t g_ui_component_enabled; /* 0x0012b4a6 */
extern uint8_t g_ui_component_latch; /* 0x001fcc43 */
extern uint8_t g_ui_active_record_scan_latch; /* 0x0012d7b3 */

extern uint16_t g_debrief_group;       /* 0x00121e98 */
extern int16_t g_match_group_count;    /* 0x001a53c0 */
extern const char g_quit_confirmation_name[5]; /* 0x00189b78 */
extern const char g_quit_notice_text[1];       /* 0x00189b5c */

/* Exact typed scalar globals used by misc_drafts1.c. */
extern uint32_t g_parser_error_context; /* 0x00121eac */
extern uint8_t g_runtime_input_byte_c; /* 0x00121e8c */
extern uint32_t g_runtime_current_tick; /* 0x00197c14 */
extern uint8_t g_runtime_coordinate_latch; /* 0x00121ec6 */
extern uint16_t g_runtime_slot_generation; /* 0x0012288a */
extern uint8_t g_marker_selection_value; /* 0x0012583b */
extern uint16_t g_scene_latch_b; /* 0x00127dae */
extern uint32_t g_texture_right_u; /* 0x00485fd8 */
extern uint32_t g_texture_right_v; /* 0x00485fdc */
extern uint32_t g_texture_left_u; /* 0x00485ff0 */
extern uint32_t g_texture_left_v; /* 0x00485ff4 */
extern uint32_t g_renderer_fixed_texture_mode; /* 0x00158f0c */
extern uint32_t g_texture_atlas_page_index; /* 0x00160000 */
extern uint32_t g_current_render_object_flags; /* 0x00420974 */
extern uint32_t g_current_render_object_clip_mask; /* 0x0042099c */
extern uint32_t g_current_render_vertex_count; /* 0x004209d0 */
extern uint8_t g_menu_presentation_disabled; /* 0x001371ee */
extern uint32_t g_data_playback_file_context; /* 0x00160644 */
extern uint32_t g_data_playback_rewind_position; /* 0x00484150 */
extern uint32_t g_data_playback_last_tick; /* 0x00484154 */
extern uint16_t g_data_playback_tick_interval; /* 0x00484466 */
extern uint16_t g_data_playback_pass; /* 0x00484468 */
extern uint8_t g_menu_animation_panel_active; /* 0x0012b0f4 */
extern uint8_t g_script_condition_automatic; /* 0x001371f9 */
extern uint16_t g_registered_callback_count; /* 0x0041eecc */
extern uint32_t g_script_record_repeat; /* 0x001600b8 */
extern uint32_t g_script_record_lookup_result; /* 0x001600c0 */
extern uint16_t g_page_record_rotation_bias; /* 0x00417222 */
extern uint16_t g_script_queue_slot; /* 0x00146e52 */
extern uint8_t g_page_value_updates_enabled; /* 0x00137a00 */
extern uint8_t g_device_event_mode; /* 0x001357c8 */
extern uint8_t g_device_event_scan_enabled; /* 0x0013583c */
extern uint8_t g_comms_initialized; /* 0x0013578c */
extern uint8_t g_comms_initialize_status; /* 0x0013578e */
extern uint16_t g_comms_initial_clock_dx; /* 0x001357d6 */
extern uint32_t g_comms_initial_tick; /* 0x00136dd4 */
extern uint8_t g_comms_initialize_flag_2; /* 0x0013581e */
extern uint32_t g_page_load_remaining; /* 0x00417160 */
extern uint8_t g_parser_state_1371fb; /* 0x001371fb */
extern uint8_t g_parser_state_1371ed; /* 0x001371ed */
extern uint8_t g_parser_state_1371ff; /* 0x001371ff */
extern uint8_t g_parser_state_137200; /* 0x00137200 */
extern uint32_t g_small_glyph_stride; /* 0x001631f4 */
extern uint16_t g_default_entry_override; /* 0x0041eede */
extern uint32_t g_ui_reset_state; /* 0x0041eeac */
extern uint8_t g_ui_reset_flag; /* 0x0041f2e7 */
extern uint8_t g_ui_state_flag_41f833; /* 0x0041f833 */
extern uint8_t g_ui_state_flag_41f2e6; /* 0x0041f2e6 */
extern uint16_t g_ui_state_word_41eed8; /* 0x0041eed8 */
extern uint8_t g_ui_pointer_device_enabled; /* 0x0041f2e1 */
extern uint8_t g_ui_state_byte_13725d; /* 0x0013725d */
extern uint8_t g_ui_state_byte_13725c; /* 0x0013725c */
extern uint8_t g_ui_state_byte_41f2e5; /* 0x0041f2e5 */
extern uint8_t g_ui_state_byte_41f2e4; /* 0x0041f2e4 */
extern uint8_t g_ui_state_byte_41f2de; /* 0x0041f2de */
extern uint16_t g_named_entry_enabled; /* 0x0041eeda */
extern uint8_t g_sound_slot_mode; /* 0x00420a70 */
extern uint8_t g_active_sound_slot; /* 0x0012dd0d */
extern uint8_t g_sound_slot_selected; /* 0x0012dd0c */
extern uint8_t g_comms_device_ready; /* 0x00135798 */
extern uint16_t g_parser_group_count; /* 0x004194c2 */
extern uint32_t g_default_file_translation_mode; /* 0x0016062d */
extern uint32_t g_record_pool_active_count; /* 0x001379b0 */
extern uint8_t g_frontend_connection_mode; /* 0x00125846 */
extern uint8_t g_frontend_network_active; /* 0x00126576 */
extern uint8_t g_frontend_pending_latch; /* 0x0041f2e8 */
extern uint8_t g_assignment_entry_count; /* 0x001a5531 */
extern uint32_t g_file_creation_permission_mask; /* 0x0017d5bc */
extern uint8_t g_session_activity_flag; /* 0x0019f5ae */
extern uint8_t g_session_first_handle_owned; /* 0x0019f59e */
extern uint8_t g_session_second_handle_owned; /* 0x0019f5a3 */
extern uint8_t g_session_state_byte; /* 0x0019f59b */
extern uint32_t g_session_first_handle; /* 0x0019f35c */
extern uint32_t g_session_second_handle; /* 0x0019f360 */
extern uint8_t g_serial_port_index; /* 0x0015f07f */
extern uint8_t g_serial_command_pending; /* 0x0015f694 */
extern uint8_t g_network_driver_mode; /* 0x0013579a */
extern uint8_t g_network_service_clear_gate; /* 0x00125902 */
extern ef_addr32 g_difficulty_row_pool; /* 0x00121e40 */
extern ef_addr32 g_allocation_callback; /* 0x0012dd9c */
extern ef_addr32 g_resource_size_hook; /* 0x00132ab4 */
extern ef_addr32 g_resource_load_hook; /* 0x00132ab8 */
extern ef_addr32 g_page_point_request_hook; /* 0x00132ad4 */
extern ef_addr32 g_positioned_record_hook; /* 0x00132ad8 */
extern ef_addr32 g_page_point_transform_hook; /* 0x00132af0 */
extern ef_addr32 g_page_point_setup_hook; /* 0x00132af4 */
extern ef_addr32 g_quoted_string_table; /* 0x00132b08 */
extern ef_addr32 g_quoted_string_storage; /* 0x00132b0c */
extern ef_addr32 g_script_record_lookup_hook; /* 0x00132b14 */
extern ef_addr32 g_comms_status_message; /* 0x00135790 */
extern ef_addr32 g_device_event_ids; /* 0x00136b84 */
extern ef_addr32 g_device_event_count_pointer; /* 0x00136b88 */
extern ef_addr32 g_active_record_pool; /* 0x001379bc */
extern ef_addr32 g_parser_active_buffer; /* 0x001379c4 */
extern ef_addr32 g_texture_shade_page; /* 0x00159084 */
extern ef_addr32 g_clock_callback; /* 0x0015b620 */
extern ef_addr32 g_video_present_callback; /* 0x0015b74c */
extern ef_addr32 g_video_prepare_callback; /* 0x0015b750 */
extern ef_addr32 g_raster_framebuffer; /* 0x0015b754 */
extern ef_addr32 g_serial_command_text; /* 0x0015f690 */
extern ef_addr32 g_script_record_parameters; /* 0x001600ac */
extern ef_addr32 g_script_record_world_position; /* 0x001600b0 */
extern ef_addr32 g_script_record_rotation; /* 0x001600b4 */
extern ef_addr32 g_renderer_vertex_base; /* 0x0017bf70 */
extern ef_addr32 g_slot_pointer_table; /* 0x001a1860 */
extern ef_addr32 g_secondary_parser_text; /* 0x001a5904 */
extern ef_addr32 g_active_page_record_data; /* 0x00417174 */
extern ef_addr32 g_active_page_point_data; /* 0x0041717c */
extern ef_addr32 g_menu_picture_path_buffer; /* 0x00417254 */
extern ef_addr32 g_menu_animation_interval_buffer; /* 0x00417258 */
extern ef_addr32 g_menu_animation_effect_buffer; /* 0x00417260 */
extern ef_addr32 g_scrolling_text_record; /* 0x004191d0 */
extern ef_addr32 g_parser_group_head; /* 0x00419200 */
extern ef_addr32 g_ui_style_bitmap_source; /* 0x0041eea8 */
extern ef_addr32 g_current_render_object_prepare; /* 0x00420990 */
extern ef_addr32 g_current_render_object_span; /* 0x00420994 */
extern ef_addr32 g_current_render_object_vertex_list; /* 0x004209bc */
extern ef_addr32 g_data_playback_screen_buffer; /* 0x0048415c */

/* Tracked camera, 0x94a74. */
extern uint16_t g_scene_update_latch; /* 0x12dba0 */
extern int32_t g_scene_distance_packed; /* 0x12dba4 */
extern int32_t g_scene_view_matrix[9]; /* 0x12dbec */
extern int32_t g_scene_subject_matrix[9]; /* 0x2143c0 */
extern uint16_t g_scene_direction_words[2]; /* 0x2143e4 */
extern int32_t g_scene_combined_matrix[9]; /* 0x214404 */
extern ef_addr32 g_scene_target; /* 0x214454 */
extern ef_addr32 g_scene_subject; /* 0x214458 */
extern int16_t g_scene_intro_timer; /* 0x12dc88, intro camera countdown */
extern ef_addr32 g_scene_follow_object; /* 0x214338, object the intro camera follows */
extern int32_t g_scene_focus_position[3]; /* 0x21432c */
extern int32_t g_scene_start_position[3]; /* 0x214320 */
extern int32_t g_scene_camera_position[3]; /* 0x21446c */
extern uint16_t g_scene_follow_angles[2]; /* 0x214464 */
extern int32_t g_scene_camera_matrix[9]; /* 0x214378 */
extern int32_t g_scene_offset_x; /* 0x214460 */
extern int32_t g_scene_offset_y; /* 0x2143fc */
extern int32_t g_scene_offset_z; /* 0x21445c */
extern int32_t g_scene_eye_position[3]; /* 0x21436c */
extern uint16_t g_scene_change_pending; /* 0x21449c */
extern uint8_t g_scene_direction_changed; /* 0x1a6d1f */

/* Completion array, 0xb00d0. */
extern uint32_t g_completion_entry_count; /* 0x1379b4 */

/* Selected view-state sync, 0x6b608. */
extern uint8_t g_view_state_sync_enabled; /* 0x122c50 */

/* Function-key camera views, 0x32c04. */
#include "ef_structs/view_keys.h"
extern ef_key_view_preset g_key_view_presets[EF_KEY_VIEW_PRESET_COUNT]; /* 0x19bf80 */
extern ef_key_view_track g_key_view_tracks[EF_KEY_VIEW_TRACK_COUNT]; /* 0x19cbb0 */
extern int16_t g_key_view_preset_cursor; /* 0x19e543 */
extern int16_t g_key_view_step; /* 0x19e545, step within the preset */
extern int16_t g_key_view_hold; /* 0x19e547, frames left in the step */
extern int32_t g_key_view_focus[3]; /* 0x19e560 */
extern int32_t g_key_view_eye[3]; /* 0x19e5b5 */
extern uint8_t g_key_view_from_preset; /* 0x19e5e4, cleared before 0x2fbc4 in 0x31514 */
extern uint32_t g_key_view_stepped_in_scene_8; /* 0x198408, set when a step ends in scene code 8 */
extern int16_t g_key_view_track_cursor; /* 0x19e549 */
extern uint8_t g_key_view_track_selected[2]; /* 0x19e5e1 */

/* Camera chain, 0x97b18. */
#include "ef_structs/camera_chain.h"
extern ef_camera_chain_record g_camera_chain_records[]; /* 0x212270, extent unknown */
extern int16_t g_camera_chain_head; /* 0x12dc74 */
extern float g_camera_chain_clock; /* 0x12dc78 */
extern float g_camera_chain_step; /* 0x132b20 */
extern double g_camera_chain_wrap; /* 0x18ac52, constant -1.0 */

/* Plane window callback, 0x69f1c. */
extern uint8_t g_saved_object_pair_marks[60]; /* 0x1f8f78, copy of 20 pair marks */
extern uint8_t g_plane_window_redrawn; /* 0x1f6f4f */
extern uint8_t g_socket_pending_ready; /* 0x00125935 */

/* Mission-role buttons, 0x4f548. */
extern uint32_t g_assignment_target_record; /* 0x1a54fa */

/* Current-mission dialog, 0x7cad4. */
extern uint8_t g_ground_effects_enabled; /* 0x12854b */
extern uint8_t g_mission_difficulty; /* 0x15b7e4 */
extern uint16_t g_mission_option_word; /* 0x12b058 */

/* Enemy view, 0x7d138. */
#include "ef_structs/unit_types.h"
extern ef_unit_type_record g_unit_type_records[]; /* 0x128564 */
extern int16_t g_enemy_view_types[]; /* 0x12afda, unit type per view slot, 0 = none */
extern int16_t g_enemy_view_page; /* 0x1f8fd8 */
extern int16_t g_enemy_view_detail; /* 0x1f8fdc */

/* Arcade high-score entry, 0x4c50. */
extern int16_t g_hiscore_new_rank; /* 0x121e48 */
extern uint16_t g_difficulty_word; /* 0x121e44, word store over g_difficulty */
extern int16_t g_arcade_mission; /* 0x121e46, 1..12 within a difficulty */
extern uint8_t g_arcade_mission_won; /* 0x121ec5 */
extern uint8_t g_score_file_name[13]; /* 0x1f8fc8, "arcXXX.txt" or a training file */

/* "kots" base selection, 0x5110. */
#include "ef_structs/kots.h"
extern uint8_t g_base_contact_blocked; /* 0x1972ef, contact while +0x12 of the object is 6 or more */
extern uint8_t g_base_contact; /* 0x1972f0, object touches a base area this frame */
extern uint8_t g_base_contact_latched; /* 0x1972f1, contact already handled */
extern uint8_t g_kots_lock; /* 0x12584b, play mode set by 0x522c: 1 solo (base buttons ignored), 2 team, 3 bases */
extern uint8_t g_kots_base_side[]; /* 0x197098, 0/1 per base */
extern uint8_t g_kots_base_side_mirror[]; /* 0x12584c, advanced and reset with the side */
extern ef_addr32 g_kots_side_labels[2]; /* 0x121e28, "GRUN" (0x9a umlaut) / "ROT" */
extern ef_kots_side_base g_kots_side_bases[2]; /* 0x121e30 */

/* Target window, 0xb5d0. */
extern ef_addr32 g_group_list_head; /* 0x137960, next link +4, record +0x13 */
extern int16_t g_target_group_index; /* 0x1976a0, -1 = none */
extern int16_t g_target_window_open; /* 0x19769a */
extern int16_t g_target_member_index; /* 0x19769c */

/* ATC flight list, 0x4f994. */
extern int16_t g_atc_slot_flights[]; /* 0x1a5722, -1 = no flight */
extern uint8_t g_atc_selected_slot; /* 0x1a5790 */

/* Map marker editing, 0x4595c. */
extern int16_t g_gui_cursor_x; /* 0x41eef4 */
extern int16_t g_gui_cursor_y; /* 0x41eef2 */
extern int16_t g_selected_map_marker; /* 0x126630, -1 = none */
extern ef_addr32 g_current_map_record; /* 0x1a4a9c, markers: count word +0x1c, 25-byte entries at +0x20 */
extern uint8_t g_map_markers_changed; /* 0x12654c */

/* Campaign target acceptance, 0xa5d8. */
#include "ef_structs/campaign_targets.h"
extern ef_campaign_target_choice g_campaign_target_choices[]; /* 0x197370 */
extern ef_saved_marker_position g_saved_marker_positions[]; /* 0x197500 */
extern int16_t g_adjacency_selected_value; /* 0x126574 */

/* Current weapon window, 0x69bdc. */
#include "ef_structs/weapon_records.h"
extern ef_weapon_record g_weapon_records[]; /* 0x12bced */
extern int16_t g_current_weapon; /* 0x1f6f4a, 0 = none */
extern int16_t g_weapon_page_flag; /* 0x1282d8 */

/* Pilot selection close, 0x43020. */
extern int16_t g_flight_origin_x; /* 0x1379a0 */
extern int16_t g_flight_origin_y; /* 0x1379a2 */
extern int16_t g_flight_window_x; /* 0x126580 */
extern int16_t g_flight_window_y; /* 0x126582 */
extern uint8_t g_performance_value; /* 0x1a364b */
extern uint8_t g_performance_display; /* 0x1a364c */

/* Campaign map mode and layer flags, 0xa6fc. */
extern uint8_t g_map_mode; /* 0x1a53dd, 0 geographical, 1 political, 2 strategic */
extern int16_t g_map_show_nato; /* 0x126554 */
extern int16_t g_map_show_russian; /* 0x126556 */
extern int16_t g_map_show_neutral; /* 0x126558 */
extern int16_t g_map_show_airdef; /* 0x12655a */
extern int16_t g_map_show_airsup; /* 0x12655c */
extern int16_t g_map_show_navy; /* 0x12655e */
extern int16_t g_map_show_comms; /* 0x126564 */
extern int16_t g_map_show_supply; /* 0x126566 */
extern int16_t g_map_show_polit; /* 0x126560 */

/* Flight-info window buttons, 0x431a4. */
extern ef_addr32 g_flightinfo_page; /* 0x12657c, 0 or a page table */
extern uint8_t g_flightinfo_mode; /* 0x126578 */
extern uint8_t g_flightinfo_toggle; /* 0x126577 */
extern int16_t g_unit_count; /* 0x19bb5c */

/* Mission-planner target list, 0x4bcd4. */
extern uint8_t g_target_list_changed; /* 0x1a54f3 */
extern int16_t g_target_type; /* 0x1a54f4 */
extern uint8_t g_target_ship_class; /* 0x1a559b, 1 awacs, 2 refueller, 3 supply, 4 kill ship */

/* Custom-mission flight slots, 0x7de5c. */
extern uint8_t g_custom_slot; /* 0x1f9009 */
extern uint8_t g_custom_slot_count[5]; /* 0x1f8ff4 */
extern int16_t g_custom_slot_type[5]; /* 0x1f8fb4, 0-based aircraft type */
extern uint8_t g_custom_slot_guns_only[5]; /* 0x1f8fea */
extern uint8_t g_custom_slot_skill[5]; /* 0x1f8ff9, 0 high, 1 medium, 2 low */
extern int16_t g_aircraft_type_text[]; /* 0x12afdc, text index per aircraft type */
extern ef_addr32 g_text_strings[]; /* 0x132dd0, localized text pointers */
extern ef_addr32 g_skill_names[3]; /* 0x12b028 */
extern int16_t g_training_mission_flag; /* 0x1f8fde, 0 for the free flight */

/* Training-mission accept, 0x7c574. */
#include "ef_structs/session_rows.h"
#include "ef_structs/environment_records.h"
extern ef_addr32 g_training_row_pool; /* 0x1f8f74, ef_session_row[] */
extern int16_t g_training_selection; /* 0x12b026, 0-based row */
extern char g_config_file_name[0x12]; /* 0x125828, current mission config name */
extern uint8_t g_custom_player_mode; /* 0x125839, 1 numbers the first custom slot from 0x1357b0 */

/* Campaign map update, 0x426a4. */
extern int16_t g_map_row_count; /* 0x1a1506 */

/* Active widget area and pointer, 0xab95c. */
extern int16_t g_active_area_x; /* 0x41eef6 */
extern int16_t g_active_area_y; /* 0x41eef8 */
extern int16_t g_active_area_width; /* 0x41eefa */
extern int16_t g_pointer_y; /* 0x41eefc, stored by the hit test */
extern int16_t g_pointer_x; /* 0x41eefe, stored by the hit test */
extern int16_t g_active_area_height; /* 0x41ef00 */
extern uint16_t g_magnifier_zoom; /* 0x137210 */

/* Sun flare script entries, 0xdb4d4. */
extern ef_addr32 g_sun_flares; /* 0x425b20, ef_sun_flare list head */
extern int16_t g_sine_table[0x2000]; /* 0x141e08, full turn; cosine at +0x800 */

/* SAM coverage, 0x4d920. */
extern ef_addr32 g_assignment_selected_record; /* 0x1a54f6 */
extern uint8_t g_sam_full_coverage; /* 0x1a5599 */
extern int16_t g_sam_coverage_width; /* 0x19e5a5 */
extern int32_t g_sam_coverage_range; /* 0x19e597 */
extern uint8_t g_assignment_current_byte; /* 0x1a5540 */
extern uint32_t g_assignment_current_value; /* 0x1a5554 */


/* Environment script (READCOLS) keyword state, 0xdad4c-0xdb8bc. */
extern int32_t g_sun_texture_count; /* 0x15bb60, "sun_texture" entries read, at most 20 */
extern int16_t g_lightning_area_count; /* 0x425bbc, at most 20 */
extern ef_lightning_area g_lightning_areas[20]; /* 0x4218e0 */
extern int32_t g_crater_count; /* 0x425ae0, "set_nb_craters" value 1 */
extern int32_t g_crater_type_count; /* 0x425ae4, value 2, at most 43 */
extern int32_t g_crater_slot_count; /* 0x425ae8, value 3 */
extern ef_addr32 g_crater_slots; /* 0x425aec, 110 bytes per slot */
extern ef_addr32 g_crater_records; /* 0x425af4, 110 bytes per crater */
extern int32_t g_flak_type_count; /* 0x425afc, -1 until "set_nb_flak" */
extern ef_cloud_level g_cloud_levels[2]; /* 0x425b64 */
extern int32_t g_cloud_level_count; /* 0x425b8c */
extern ef_heavenly_body g_heavenly_bodies[40]; /* 0x422398 */
extern uint8_t g_horizon_fixed; /* 0x425c86, set by "fix_horizon", cleared by "unfix_horizon" */
extern int16_t g_heavenly_size_squared; /* 0x425ba2, "heavenly_body" value 4 squared */
extern int16_t g_heavenly_value5; /* 0x425ba4, "heavenly_body" value 5 */
extern int16_t g_heavenly_value3; /* 0x425bb0, "heavenly_body" value 3 */
extern int16_t g_heavenly_value2; /* 0x425bb4, "heavenly_body" value 2 */
extern int16_t g_heavenly_shape; /* 0x425bb8, shape number of "<name>.3" */
extern ef_addr32 g_env_script_cursor; /* 0x425af8, next unread environment script line */
extern char g_coltab_name[13]; /* 0x17d368, "coltab" name with ".dat" */
extern char g_init3d_name[13]; /* 0x15bb64, "init3d_file" name */
extern uint8_t g_east_west_palettes_loaded; /* 0x15bb74, set by "east_west_palettes" */
extern ef_env_keyword g_game_type_keywords[15]; /* 0x15bd40, 14 game types and a sentinel */
extern int16_t g_palette_count; /* 0x425bca, "load_pal" lines read */
extern uint8_t g_palette_banks[12][0x300]; /* 0x422760 */
extern uint8_t g_shade_tables[12][0x4004]; /* 0x433f30, loaded with each palette */
extern int32_t g_palette_values[12]; /* 0x424e60, "load_pal" value per palette */
extern int32_t g_first_palette_value; /* 0x425b98 */
extern uint8_t g_current_palette[0x300]; /* 0x464060 */
extern ef_ref_point g_ref_points[256]; /* 0x421a48 */
extern uint8_t g_ref_point_count; /* 0x425c87, wraps after 256 */
extern ef_addr32 g_crater_ring; /* 0x425b88, first of g_crater_records */
extern ef_addr32 g_crater_slot_ring; /* 0x425b24, first of g_crater_slots */
extern ef_addr32 g_environ; /* 0x160a18, null-terminated "NAME=value" pointers */
extern ef_addr32 g_environ_owned; /* 0x160a1c, one heap-ownership flag per environment entry */
extern int32_t g_mouse_motion_x; /* 0x420dc4, motion since the last poll */
extern int32_t g_mouse_motion_y; /* 0x420dc8 */
extern int16_t g_menu_cursor_y; /* 0x425fc6, menu cursor on the 320x200 screen */
extern int16_t g_menu_cursor_x; /* 0x425fc8 */
extern int32_t g_cursor_repeat_x; /* 0x161e24, ticks until the next slow step */
extern int32_t g_cursor_repeat_y; /* 0x161e28 */
extern int32_t g_directplay_selected_game; /* 0x42920c, zero-based row of g_directplay_rows */
extern char g_directplay_rows[][64]; /* 0x4288f0, one 64-byte name per DirectPlay game */
extern uint8_t g_mouse_button_down; /* 0x15ba2c */
extern ef_addr32 g_input_poll_hook; /* 0x15bab8, called without arguments before reading the motion */

extern int16_t g_watched_unit; /* 0x1f71ca, unit whose events raise alerts and player messages */
extern uint8_t g_message_filter; /* 0x1f9010, only event kinds 0x4d/0x4f reach the player while set */
extern uint8_t g_watched_unit_alert; /* 0x1f9011, set by 0x46254 */

extern uint8_t g_route_selected_slot; /* 0x1a559f, 0x32 when none */
extern int16_t g_route_slot_x[26]; /* 0x1a55a2 */
extern int16_t g_route_slot_y[26]; /* 0x1a55d6 */
extern uint8_t g_unit_tally_member_seen; /* 0x121ec0 */
extern uint32_t g_cursor_ray_from_y; /* 0x1fcc18 */
extern int16_t g_cursor_screen_shift_x; /* 0x1fcbd0 */
extern int16_t g_cursor_screen_shift_y; /* 0x1fcbd2 */
extern int32_t g_cursor_target_x; /* 0x1f8eb0 */
extern int32_t g_cursor_target_y; /* 0x1f8eac */
extern int32_t g_cursor_target_z; /* 0x1f8ea8 */
extern int32_t g_draw_clip_left; /* 0x17cf9c */
extern int32_t g_draw_clip_right; /* 0x17cfa0 */
extern int32_t g_draw_clip_top; /* 0x17cfa4 */
extern int32_t g_draw_clip_bottom; /* 0x17cfa8 */
extern int16_t g_element_rect_x; /* 0x41ec00 */
extern int16_t g_element_rect_y; /* 0x41ec02 */
extern int16_t g_element_rect_w; /* 0x41ec04 */
extern int16_t g_element_rect_h; /* 0x41ec06 */
extern int16_t g_element_clip_x; /* 0x41ec08 */
extern int16_t g_element_clip_y; /* 0x41ec0a */
extern int16_t g_element_clip_w; /* 0x41ec0c */
extern int16_t g_element_clip_h; /* 0x41ec0e */

#endif
