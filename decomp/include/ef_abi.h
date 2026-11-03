/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_ABI_H
#define EF_ABI_H

#include "ef_types.h"

/* Parser and tactical-activity helpers, host drafts pending guest proof. */
uint32_t mirror_reference_state(void); /* 0x37294 */
void normalize_script_line_endings(ef_addr32 text); /* 0x3cc28 */
void append_tactical_ring_event(int16_t x, int16_t y, uint8_t code); /* 0x41f64 AX/DX/BL */
void copy_next_text_line(void); /* 0x50ca4 */
uint32_t count_matching_activity_slots(ef_addr32 object); /* 0x5aa10 EAX */
uint8_t active_unit_record_contains(int16_t index); /* 0x62548 AX -> AL */
uint16_t next_round_robin_record(void); /* 0x631bc -> AX */
void clear_runtime_transient_counters(void); /* 0x79524 */

/* Caller-side state and coordinate wrappers; all remain host drafts. */
double convert_signed_short_scale(int16_t value); /* 0x53308 AX -> ST0 */
uint8_t grid_clock_reaches_reference(int32_t row); /* 0x855c8 full EAX -> AL */
void refresh_object_child_slot(ef_addr32 object); /* 0x88100 EAX */
uint8_t first_nonignored_activity_slot(int16_t object); /* 0x860c0 AX -> AL */
uint16_t advance_panel_strip(void); /* 0x80abc -> AX */
void transform_child_coordinates(ef_addr32 object, ef_addr32 output); /* 0x88c80 EAX/EDX */
void transform_signed_angle_pair(ef_addr32 object, ef_addr32 output_a,
                                 ef_addr32 output_b); /* 0x88d4c EAX/EDX/EBX */

/* UI dispatcher cluster, original entry points 0x7fbec..0x80400. */
void ui_dispatch_indexed_control(uint8_t code, int32_t index,
                                  int32_t value, int32_t extra); /* 0x7fbec */
void ui_initialize_releases(void); /* 0x7fe18 */
void ui_finalize_releases(void); /* 0x7fe4c */
void ui_release_all_controls(void); /* 0x7fe8c */
void ui_release_matching_control(uint16_t code); /* 0x802ac */
void ui_reset_active_panel(void); /* 0x803c4 */
void ui_reset_panel_and_release(void); /* 0x80400 */
void ui_emit_selected_message(void); /* 0x42548 */

/* Original helper call interfaces, retained as guest dependencies. */
void ui_control_activate(uint32_t index); /* 0xd7490, EAX */
void ui_control_release(uint32_t index); /* 0xd74d0, EAX */
void ui_control_deactivate(uint32_t index); /* 0xd7530, EAX */
void ui_control_update(uint32_t arg_a, uint32_t arg_d,
                       uint32_t arg_b, uint32_t arg_c); /* 0xd7158, EAX/EDX/EBX/ECX */
void ui_reset_panel_memory(ef_addr32 address); /* 0xd6ffc, EAX */
void ui_report_out_of_range(int32_t index); /* 0x132a74 indirect call */
void ui_show_message(ef_addr32 address); /* 0xa6018, tailcall */

/* Packet writers, two status formatters and guest-side panel refresh. */
void send_packet_15(int16_t ax, int16_t dx, uint8_t tag, uint32_t value,
                    uint32_t stack_a, uint32_t stack_b); /* 0x548a4, ret 8 */
void send_packet_41(int16_t dx, int16_t bx, uint8_t code,
                    uint16_t stack_index); /* 0x55b44, ret 4 */
void send_packet_2b(int16_t ax, int16_t dx, int16_t bx); /* 0x56974 */
void write_ui_status_flags_a(ef_addr32 destination); /* 0x82790 EAX */
void write_ui_status_flags_b(ef_addr32 destination); /* 0x827d0 EAX */
void write_ui_status_float(ef_addr32 destination); /* 0x82bd4 EAX */
void draw_tactical_item_mode(ef_addr32 item); /* 0x59484 EAX */
void refresh_tactical_indicator(void); /* 0x5331c */

/* Original guest sinks for status formatting and tactical drawing. */
void format_ui_status_pair(ef_addr32 destination, ef_addr32 format,
                           uint32_t first, uint32_t second); /* 0xa4342 */
void format_ui_status_float(ef_addr32 destination, ef_addr32 format,
                            double value); /* 0xa4342 */
int32_t draw_tactical_mode_item(uint32_t mode, ef_addr32 item); /* 0x48c34 */
void read_tactical_indicator_callback(void); /* indirect 0x15bab8 */

/* Runtime record and tactical display operations, separate original sinks. */
void draw_tactical_coordinate_marker(uint8_t mark, uint32_t mode,
                                     int32_t x, int32_t y, uint32_t aux); /* 0x48bb0 */
int32_t draw_tactical_item_metric(ef_addr32 item); /* 0x594a4 EAX */
void draw_tactical_coordinates(int16_t x, int16_t y, uint32_t aux); /* 0x59420 AX/DX/EBX */
void update_position_bin(void); /* 0x66ac8 */
uint16_t bin_coordinate_to_slot(int32_t coordinate); /* 0x663a4 EAX, EDX=0 */
void report_tactical_object_action(ef_addr32 object); /* 0x741f0 EAX */
void notify_tactical_action(int32_t index, uint32_t action); /* 0x9e508 EAX/EDX */
ef_addr32 build_status_description(ef_addr32 suffix); /* 0x7a3b0 EAX */
void guest_string_copy(ef_addr32 dest, ef_addr32 src); /* 0x9d749 EAX/EDX */
void guest_string_append(ef_addr32 dest, ef_addr32 src); /* 0x9ca57 EAX/EDX */
void refresh_record_selection(ef_addr32 record); /* 0x87970 EAX */
uint32_t original_record_selection_check(ef_addr32 record); /* 0x866e0 EAX */
void rotate_ui_record_ring(void); /* 0x879c4 */
void initialize_ui_record_ring(ef_addr32 head); /* 0x878c8 EAX */
void reset_ui_record_pool(void); /* 0x8f5f4 */

/* UI string, tactical and record caller wrappers. Guest sinks are original. */
void send_packet_2c(uint8_t a, uint8_t d, uint8_t b, uint8_t c,
                    uint8_t stack_code); /* 0x577ac AL/DL/BL/CL, ret 4 */
void guest_packet_after_send(const uint8_t *packet); /* 0x57820 EAX */
void draw_tactical_cross(int16_t x, int16_t y); /* 0x5dbc8 AX/DX */
void draw_tactical_line(int32_t y1, int32_t x1, int32_t y2,
                        int32_t x2, uint32_t marker); /* 0xc8ccc cdecl */
void update_ui_record_from_template(int16_t index, ef_addr32 record); /* 0x80d40 AX/EDX */
ef_addr32 lookup_ui_record_template(ef_addr32 table, int32_t index); /* 0x80d20 EAX/EDX */
void update_ui_composite(void); /* 0x84370 */
void compose_ui_relative_vector(ef_addr32 base, uint32_t d,
                                uint32_t b, uint32_t c); /* 0x83708 */
uint32_t ui_record_current_is_active(ef_addr32 record); /* 0x866e0 EAX */
void ui_report_missing_record(ef_addr32 message); /* indirect 0x132a74 */
void update_object_with_link(ef_addr32 object, ef_addr32 link); /* 0x8be54 EAX/EDX */
void guest_object_unlink_if_current(ef_addr32 object, ef_addr32 link); /* 0x8f5d8 */
void guest_object_install_link(ef_addr32 object, ef_addr32 link); /* 0x90924 */
void compact_ui_fragment(ef_addr32 string); /* 0x82dd0 EAX */
ef_addr32 find_matching_ui_fragment(ef_addr32 list, ef_addr32 needle); /* 0x82e00 EAX/EDX */
ef_addr32 next_ui_fragment(ef_addr32 string); /* 0x82da4 EAX */
uint32_t guest_string_length(ef_addr32 string); /* 0xb1b8b EAX */
int32_t guest_prefix_compare(ef_addr32 a, ef_addr32 b,
                             uint32_t length); /* 0xe5c03 EAX/EDX/EBX */

/* Further small UI and object-state functions and their original sinks. */
uint8_t ui_point_near_reference(int32_t x, int32_t y); /* 0x83294 EAX/EDX */
int32_t guest_ui_distance_metric(int32_t dx, int32_t dy); /* 0xb3988 EAX/EDX */
void guest_prepare_ui_vector(ef_addr32 base, uint32_t d,
                             uint32_t b, uint32_t c); /* 0x83200 */
uint8_t initialize_ui_component(void); /* 0x84514 -> AL=0 */
void guest_copy_ui_component(ef_addr32 dest, ef_addr32 src,
                             uint32_t length); /* 0xdc832 EAX/EDX/EBX */
void recalculate_object_temporarily(ef_addr32 object); /* 0x89924 EAX */
void guest_recalculate_object(ef_addr32 object); /* 0x8a318 EAX */
void update_object_remove_link(ef_addr32 object, ef_addr32 link); /* 0x8be3c EAX/EDX */
void guest_object_prepare_unlink(ef_addr32 object, ef_addr32 link); /* 0x8f3f4 */
void guest_object_apply_unlink(ef_addr32 object, ef_addr32 link); /* 0x90880 */
ef_addr32 select_validated_ui_record(ef_addr32 owner); /* 0x8b650 EAX */
ef_addr32 guest_read_candidate_record(void); /* indirect 0x132ab4 */
ef_addr32 guest_convert_candidate_record(ef_addr32 source); /* 0xe29e8 EAX */
uint8_t guest_validate_candidate_record(ef_addr32 owner, ef_addr32 candidate); /* 0x132ab8 */
void prepare_navigation_component(ef_addr32 object, ef_addr32 predicate); /* 0x92254 EAX/EDX */
void guest_prepare_navigation_component(ef_addr32 buffer, ef_addr32 predicate,
                                       ef_addr32 object); /* 0xdc91c EAX/EDX/EBX */

/* Command-file parser and six-field record decoder; source is the game's
 * global 0x1976b0 buffer, token scratch is 0x197abe (both guest RAM). */
void release_parser_buffers(void); /* 0xe710 */
void initialize_parser_records(void); /* 0xe7dc */
uint32_t read_parser_token(void); /* 0xe89c -> EAX length */
uint32_t read_parser_decimal(void); /* 0xe980 -> EAX guest conversion result */
int32_t read_parser_integer(void); /* 0xe95c -> EAX or -1 for empty */
ef_addr32 duplicate_parser_token(void); /* 0xe990 -> EAX allocated text */
uint32_t read_parser_enum(void); /* 0xe9bc -> EAX enum index */
uint32_t read_parser_six_field_record(ef_addr32 destination); /* 0xee5c EAX -> EAX=1 */
uint32_t read_parser_enum_two_families(void); /* 0xea0c -> EAX 0..3 */
uint32_t read_parser_enum_30_names(void); /* 0xeaf8 -> EAX index */
uint32_t read_parser_enum_39_names(void); /* 0xeb48 -> EAX index */
uint32_t read_parser_enum_18_names(void); /* 0xeb9c -> EAX index */
uint32_t read_parser_enum_27_names(void); /* 0xebec -> EAX index */
uint32_t read_parser_enum_106_names(void); /* 0xec8c -> EAX index */
uint32_t read_parser_enum_10_names(void); /* 0xece0 -> EAX index */
uint32_t read_parser_enum_19_names(void); /* 0xed30 -> EAX index */
uint8_t read_parser_line(void); /* 0xe810 -> AL first byte of line */
void release_secondary_parser_text(void); /* 0xf8f8 */
uint32_t read_parser_text_record(ef_addr32 record); /* 0xfe4c EAX -> EAX=1 */
uint8_t guest_parser_next_character(void); /* 0xe774 -> AL */
void guest_parser_line_overflow(ef_addr32 message); /* [0x132a74], one stack arg */

/* Small runtime state, clock, packed-coordinate and word-pair helpers. */
void copy_runtime_input_state(ef_addr32 word_a, ef_addr32 word_b,
                              ef_addr32 byte_c); /* 0xd530 EAX/EDX/EBX */
uint32_t sample_runtime_tick(void); /* 0x10640 -> EAX */
int32_t advance_runtime_tick(void); /* 0x1065c -> EAX >=2 */
uint32_t guest_read_runtime_tick(void); /* [0x15b620] -> EAX */
void pack_runtime_position(int16_t heading, int32_t x,
                           int32_t y, uint8_t code); /* 0x106b0 AX/EDX/ECX/stack, ret4 */
void transform_runtime_word_pair(uint16_t mode, ef_addr32 a,
                                 ef_addr32 b); /* 0x10f28 AX/EDX/EBX */

/* Indexed control and event/dialog wrappers. */
uint32_t encode_runtime_segment_offset(ef_addr32 pointer); /* 0x604 EAX -> EAX */
void reinitialize_runtime_slots(void); /* 0x12160 */
uint8_t read_runtime_slot_flag(int16_t subindex, int16_t group); /* 0x121cc AX/DX -> AL */
void initialize_runtime_probe_latch(void); /* 0x121f0 */
void guest_invoke_runtime_slot(ef_addr32 callback); /* indirect record+25 */
void guest_refresh_runtime_slot(int16_t index); /* 0x11e7c EAX */
void guest_query_runtime_pair(uint32_t *first, uint32_t *second); /* 0xce02c EAX/EDX */
void refresh_all_event_slots(void); /* 0x14ef4 */
void guest_refresh_event_slot(int16_t index); /* 0x14d58 EAX */
void dispatch_two_object_events(ef_addr32 object, int16_t index); /* 0x1d5cc EAX/DX */
void guest_dispatch_object_event_optional(ef_addr32 object, int16_t index); /* 0x55dac */
void guest_dispatch_object_event_required(ef_addr32 object, int16_t index); /* 0x1d5f4 */
void update_dialog_bindings(ef_addr32 first, ef_addr32 second); /* 0x4f314 EAX/EDX */
void flush_active_dialog_slots(void); /* 0x4f6bc */
ef_addr32 guest_open_named_dialog(ef_addr32 name, ef_addr32 root); /* 0xa46cc */
ef_addr32 guest_select_dialog_widget(ef_addr32 root, ef_addr32 name); /* 0xa78bc */
void guest_bind_dialog_widget(ef_addr32 widget, ef_addr32 source); /* 0xa7588 */
void guest_emit_dialog_flag(uint8_t flag); /* 0xa4cb4 AL */
void guest_destroy_named_dialog(ef_addr32 dialog); /* 0xac9a0 */

/* Unrecovered original conversion/allocation/diagnostic interfaces. */
void guest_release_parser_buffer(ef_addr32 buffer); /* 0xcc768 EAX */
uint32_t guest_convert_parser_decimal(ef_addr32 text); /* 0xcc76d EAX */
int32_t guest_convert_parser_integer(ef_addr32 text); /* 0x9d4cd EAX */
ef_addr32 guest_allocate_parser_text(uint32_t bytes, ef_addr32 source); /* [0x121ebc] */
int32_t guest_compare_parser_token(ef_addr32 token, ef_addr32 candidate); /* 0xa4380 */
void guest_report_parser_enum_error(ef_addr32 format, ef_addr32 token,
                                    uint32_t context); /* [0x132a74], cdecl */

/* Short clock, tactical state and coordinate helpers (host drafts). */
uint8_t reference_clock_reaches(ef_addr32 guest_clock); /* 0x40820 EAX -> AL */
uint32_t command_code_is_group_1b(uint8_t code); /* 0x41da4 AL -> EAX */
void clear_tactical_accumulators(void); /* 0x49054 */
void reset_tactical_slots(void); /* 0x4d8f4 */
void advance_first_tactical_status(void); /* 0x4dc98 */
void count_external_grid_markers(void); /* 0x544c0 */
uint8_t position_within_tactical_bounds(int16_t x, int16_t y); /* 0x5cb9c AX/DX -> AL */
void sample_active_coordinates(ef_addr32 destination); /* 0x5f1ec EAX */

/* Arithmetic, grid, UI-list and strip helpers (host drafts). */
int32_t clock_words_to_seconds(ef_addr32 clock); /* 0x40774 EAX -> EAX */
uint8_t tactical_lookup_contains(uint8_t code); /* 0x50a1c AL -> AL */
uint8_t clock_triplet_is_present(ef_addr32 clock); /* 0x85144 EAX -> AL */
void decrement_grid_activity(int16_t row); /* 0x85358 AX */
uint32_t reset_ui_panel_selection(void); /* 0x86920 returns EAX=5 */
ef_addr32 find_ui_linked_tag(uint8_t tag, int16_t value); /* 0x87f9c AL/DX -> EAX */
ef_addr32 mark_ui_links_by_slot(int16_t slot); /* 0x8d0e0 AX -> EAX */
int32_t clear_panel_strip(int16_t slot); /* 0x809bc AX -> EAX */

/* The game generally uses Watcom register calling, but individual signatures
 * must be confirmed from callers before adding #pragma aux definitions here.
 * See symbols/signatures.tsv and docs/decompilation-plan.md §3.5. */

/* 0x000432c: no arguments, only AL observed as output (two callers). */
uint8_t hiscore_qualifies(void);

/* 0x000263e0: no arguments or register return; preserves all GPRs. */
void reset_spatial_slot_count(void);

/* 0x000263ec: inputs AX/DX/BL; no observed register return. */
void append_spatial_slot(int16_t x, int16_t y, uint8_t tag);

/* 0x0006a1b4: input AX (signed), output full EAX guest address. */
ef_addr32 lookup_indexed_object(int16_t index);

/* 0x0007489c: AX signed index, EAX pointer/zero; increments a diagnostic
 * counter on every call, including rejected indices. */
ef_addr32 lookup_filtered_indexed_object(int16_t index);

/* Original filtered-lookup wrappers, signed AX input; no return consumed. */
void set_indexed_object_activation(int16_t index);    /* 0x0006a354 */
void clear_indexed_object_activation(int16_t index);  /* 0x0006a378 */
void set_indexed_object_state(int16_t index); /* 0x0006a394 */
void clear_indexed_object_state(int16_t index); /* 0x0006a3d0 */

/* World grid / indexed-object state: drafted only, not guest verified. */
uint8_t grid_slot_matches_object(int16_t index); /* 0x0002b884: AL result */
void set_grid_slot_event(int16_t cell, uint8_t value); /* 0x0002b910: AX,DL */
void enable_grid_slot_state(int16_t cell); /* 0x0002b95c: AX */
void enable_grid_slot_activation(int16_t cell); /* 0x0002ba14: AX */
void set_indexed_object_mode(int16_t index, uint8_t mode); /* 0x0002541c: AX,DL */

/* These are original guest dependencies. Calls are typed from the specific
 * original call sites, not independent recovered implementations. */
uint8_t check_current_object_relation(void); /* 0x00023800, AL */
void dispatch_indexed_object_event(int16_t index, int16_t event,
                                    int32_t aux_b, int32_t aux_c,
                                    int32_t stack_arg); /* 0x00046168, ret 4 */
void process_object_state_29(ef_addr32 object); /* 0x0002a168 */
void process_object_state_30(ef_addr32 object); /* 0x0002a1c4 */
void sync_current_object_slot(void); /* 0x000238a0 */

/* Second world-grid batch: signed AX row inputs unless stated otherwise. */
void link_grid_cell(int16_t cell, int16_t neighbor); /* 0x0002b558: AX,DX */
uint8_t grid_has_active_link(int16_t cell); /* 0x0002beb0: AL result */
uint8_t grid_has_object_mode(int16_t cell, uint8_t mode); /* 0x0002bf08: AX,DL; AL */
uint8_t grid_is_current_or_external(int16_t cell); /* 0x0002dba4: AL */
void reset_grid_cell_state(int16_t cell); /* 0x0002e030 */
uint8_t grid_event_matches(uint8_t code, uint8_t mode); /* 0x0002e910: AL,DL; AL */

/* Unrecovered original dependencies, typed at these particular call sites. */
void set_grid_link_state(int16_t cell, int16_t mode); /* 0x0002b1f4 */
uint8_t external_grid_link_matches(int16_t cell, uint8_t external_index); /* 0x0002db64 */
void reset_grid_cell_derived(int16_t cell); /* 0x0002e08c */
void update_grid_visual_state(int16_t cell); /* 0x00054c10 */

/* Grid helper cluster: signed AX row inputs; DX or DL as noted. */
void detach_grid_link_on_mode_change(int16_t cell, uint8_t next_mode); /* 0x0002afcc AX,DL */
void link_external_grid_cell(int16_t cell, int16_t node); /* 0x0002b5c0 AX,DX */
void copy_grid_link_position(int16_t cell); /* 0x0002b634, now recovered */
void refresh_external_grid_position(int16_t cell); /* 0x0002b690 */
void recompute_grid_limit(int16_t cell); /* 0x0002e08c */

/* Grid adjacency, relationship, category and visibility helpers. The x87
 * distance models still require guest differential precision checks. */
void recycle_oldest_grid_node(void); /* 0x0002a934, no arguments */
int16_t find_grid_neighbor_with_detail(int16_t cell); /* 0x0002bbd0, AX -> AX */
double grid_cell_distance(int16_t first, int16_t second); /* 0x0002bb3c, AX/DX -> ST0 */
double grid_neighbor_distance(int16_t cell); /* 0x0002bc3c, AX -> ST0 */
uint8_t grid_has_cross_object_relation(int16_t cell); /* 0x0002c0ec, AX -> AL */
void refresh_grid_category(int16_t cell); /* 0x0002da54, AX */
uint8_t grid_is_visible_from_reference(int16_t cell); /* 0x0002dce0, AX -> AL */

/* Object geometry and limit helpers (drafted, not guest-spliced). */
uint8_t object_kind_is_eligible(uint8_t kind); /* 0x6b3d0, AL -> AL */
double object_type_limit(ef_addr32 object); /* 0x6b4ac, EAX -> ST0 */
uint8_t lower_object_limit(ef_addr32 object); /* 0x6b54c, EAX -> AL */
void reset_object_limit(int16_t object); /* 0x6b58c, AX */
double object_speed_from_clock(ef_addr32 a, ef_addr32 b, ef_addr32 clock); /* 0x6b5a8, EAX/EDX/EBX -> ST0 */
double indexed_object_distance(int16_t a, int16_t b); /* 0x6a5f0, AX/DX -> ST0 */
double grid_distance_to_point(ef_addr32 point, int16_t cell); /* 0x2bab0, EAX/DX -> ST0 */
double scaled_grid_distance_to_point(ef_addr32 point, int16_t cell); /* 0x2bae0, EAX/DX -> ST0 */

/* Drafted 0x2949c: AX object, DL feature, AL slot/20; signed-code quirk. */
uint8_t query_indexed_object_feature(int16_t object, uint8_t code);

/* Linked scene-node helpers (0x90000 region). */
uint8_t scene_node_is_registered(ef_addr32 node); /* 0x90050, EAX -> AL */
void clear_scene_node_children(int16_t index); /* 0x90074, AX */
void dispatch_scene_node_event(int16_t index, int16_t event, uint8_t code); /* 0x900c0, AX/BX/DL */
void visit_all_scene_nodes(void); /* 0x901ec, no arguments */
ef_addr32 next_scene_node(ef_addr32 node); /* 0x9065c, EAX -> EAX */
uint8_t scene_node_has_active_children(int16_t index); /* 0x906b4, AX -> AL */
ef_addr32 next_scene_node_with_active_children(ef_addr32 node); /* 0x907f0, EAX -> EAX */

/* 0x8ffe8: signed AX scene-node slot, recovered in scene_indexed.c. */
ef_addr32 lookup_scene_node(int16_t index);
void notify_scene_owner(int16_t index); /* 0x3739c, signed AX */
void dispatch_scene_node_direct(int16_t index, int16_t event, uint8_t code); /* 0x90124 */
void dispatch_scene_node_external(int16_t index, int16_t event, uint8_t code); /* 0x55e1c */
void visit_scene_node_standard(ef_addr32 node); /* 0x9021c */
void visit_scene_node_extended(ef_addr32 node); /* 0x90378 */

/* State/angle and query helpers surrounding 0x9125c..0x91c97. */
int16_t scene_lookup_named_word(int32_t key); /* 0x9125c, full EAX then AX -> AX */
int32_t scene_metric_remaining(void); /* 0x91298, no arguments, EAX */
uint8_t scene_metric_reaches_threshold(int32_t ignored, int32_t threshold); /* 0x918f0, EAX/EDX -> AL */
void set_scene_input_triplet(int32_t x, int32_t y, int32_t z); /* 0x91910, EAX/EDX/EBX */
void set_scene_angles(int16_t x, int16_t y, int16_t z); /* 0x9195c, AX/DX/BX */
void request_scene_pair(int32_t left, int32_t right); /* 0x91acc, EAX/EDX */
void clear_scene_event_counters(void); /* 0x91c20 */
void record_scene_event(int32_t packed_event); /* 0x91c48, EAX */

/* Calls into original game/runtime; independent implementations not recovered. */
ef_addr32 find_scene_named_group(ef_addr32 name, int32_t key); /* 0xa1a40, EAX/full EDX */
ef_addr32 find_scene_named_entry(int16_t key, ef_addr32 group); /* 0xa1a90, EAX/EDX */
int32_t evaluate_scene_metric(int32_t initial, ef_addr32 right, ef_addr32 left); /* 0x7b16c, EAX/EDX/EBX */
void configure_scene_angles(int16_t z, int16_t y, int16_t x); /* 0xc4654, stack args */
uint8_t probe_scene_pair_state(int32_t base, int32_t kind, int32_t left, int32_t right); /* 0x913dc, EAX/EDX/EBX/ECX */
void perform_scene_pair_request(int16_t left, int16_t right); /* 0x91afc, AX/DX */
void scene_event_callback(void); /* 0x86920, original side effects */

/* Indexed scene / object-link queries and mutations. */
uint32_t classify_object_percentage(uint8_t percentage); /* 0x75620, AL -> full EAX */
uint8_t object_uses_secondary_grid(int16_t index); /* 0x7574c, AX -> AL */
void reset_object_link_controls(void); /* 0x75c84 */
void attach_object_link_child(ef_addr32 parent, int16_t child, uint8_t slot); /* 0x76b30, EAX/BX/DL */
void configure_navigation_record(ef_addr32 source); /* 0x78aa8, EAX */
uint8_t indexed_object_has_grid_link(int16_t index); /* 0x6ae30, AX -> AL */
void clear_paired_object_marks(int16_t index); /* 0x68c18, AX */

/* Guest-only dependencies of the above helpers, not recovered C functions. */
void copy_navigation_record(ef_addr32 target, ef_addr32 source); /* 0x9d749, EAX/EDX */
int16_t check_special_object_grid_link(int16_t index); /* 0x6aa70, AX -> AX */
uint8_t dispatch_object_grid_link(int16_t index, int16_t linked); /* 0x6abac, AX/EDX -> AL */
void clear_object_pair_reference(int16_t root, int16_t other, int32_t sentinel); /* 0xe7c50, EAX/EBX/EDX */

/* Buffer cursor / planar lookup and indexed-object identity helpers. */
void check_buffer_capacity(void); /* 0x7a480, no inputs */
void buffer_write_word(int16_t value); /* 0x7a584, AX */
int32_t buffer_read_word(void); /* 0x7a5a0, EAX signed */
ef_addr32 select_plane_source(int32_t mode); /* 0x7aac0, EAX */
uint32_t read_plane_cell(int32_t mode, int32_t row, int32_t column); /* 0x7b2f4, EAX/EDX/EBX -> EAX */
uint8_t indexed_object_is_canonical(ef_addr32 object); /* 0x8c09c, EAX -> AL */
ef_addr32 indexed_object_direct_slot(int16_t slot); /* 0x8c0bc, AX -> EAX; unguarded */
void request_scene_refresh(void); /* 0x95018 */
void report_buffer_capacity_exceeded(ef_addr32 message); /* original indirect call via 0x132a74 */

/* Scene controls, selectors and packed angle record. */
void configure_scene_feature(int16_t index); /* 0x8f620, AX */
void set_secondary_object_byte(uint8_t value); /* 0x8bf00, AL */
uint8_t scene_gate_allows(int32_t value); /* 0x8bf1c, EAX -> AL */
void configure_scene_angle_triplet(int16_t first, int16_t second, int16_t third); /* 0x91d9c, AX/DX/BX */
/* Calls made by 0x91d9c; original implementations remain guest-only. */
void initialize_scene_angle_record(ef_addr32 record); /* 0xd0b68 */
void update_scene_angle_first(ef_addr32 record, uint16_t value); /* 0xd0ebc */
void update_scene_angle_second(ef_addr32 record, uint16_t value); /* 0xd0dbc */
void update_scene_angle_third(ef_addr32 record, uint16_t value); /* 0xd0fb8 */

/* Three-component x87 helpers: signed dwords, double-precision guest RAM.
 * Host C results are provisional until x87 extended-precision replay. */
void subtract_scene_vectors(ef_addr32 output, ef_addr32 subtract, ef_addr32 from); /* 0x88d30, EAX/EDX/EBX */
void convert_scene_scalar(ef_addr32 source, ef_addr32 output); /* 0x88d84, EAX/EDX */
void convert_scene_vector(ef_addr32 source, ef_addr32 output); /* 0x88c54, EAX/EDX */
void convert_navigation_vector(ef_addr32 source, ef_addr32 output); /* 0x8fa58, EAX/EDX */

/* Link resolution and short state-setting functions. */
ef_addr32 resolve_link_pointer(int16_t index, ef_addr32 candidate); /* 0x38658, AX/EDX -> EAX */
ef_addr32 refresh_link_pointer(ef_addr32 reference); /* 0x3856c, EAX -> EAX */
void store_runtime_control_word(uint32_t value); /* 0x3cd64, EAX */
void set_runtime_phase_five(void); /* 0x3dcc0 */
void initialize_mode_limits(void); /* 0x58d24 */
void reset_unit_counter(void); /* 0x5e658 */
uint32_t copy_active_buffer_pointer(void); /* 0x7a428, EAX */
uint8_t set_control_mode_a(uint8_t value); /* 0x7eea0, AL -> EAX zero-extended */
uint8_t set_control_mode_b(uint8_t value); /* 0x810d0, AL -> EAX zero-extended */
uint8_t set_control_mode_c(uint8_t value); /* 0x810f4, AL -> EAX zero-extended */

/* Binary command-packet emission through guest helper 0xa3f74. */
void emit_command_packet(const uint8_t *bytes, uint32_t size); /* guest-only 0xa3f74, EAX/EDX */
void send_object_state_packet(int16_t index, uint8_t value); /* 0x56f0c, AX/DL */
void send_object_code_39(int16_t index); /* 0x57140, AX */
void send_object_code_3a(int16_t index); /* 0x57198, AX */
void send_object_code_4a(int16_t index); /* 0x57684, AX */
void send_object_code_4b(int16_t index); /* 0x576b4, AX */
void send_object_code_4c(int16_t index); /* 0x576e4, AX */
void send_grid_marker_packet(int16_t index, uint8_t value); /* 0x545b8, AX/DL */
void send_grid_marker_short_packet(int16_t index); /* 0x546c0, AX */

/* Original dependencies at 0x2dc84/0x2dbf8; no separately confirmed ABIs. */
uint8_t grid_reference_distance_matches(int16_t cell); /* 0x0002dc84 */
uint8_t grid_external_distance_matches(int16_t cell, uint8_t probe); /* 0x0002dbf8 */

/* Marker packet wrappers and grid-row marker store. */
void send_command_1d(void); /* 0x545e4 */
void send_marker_packet_26(void); /* 0x54600 */
void send_marker_packet_1e(void); /* 0x54624 */
void send_command_1f(void); /* 0x54648 */
void send_word_byte_packet_24(int16_t value, uint8_t flag); /* 0x54664 AX/DL */
void send_marker_packet_1b(void); /* 0x54e5c */
void commit_marker_selection(ef_addr32 record); /* 0x54e9c EAX */
void apply_marker_to_grid_row(ef_addr32 record); /* 0x550b0 EAX */

void send_two_word_packet_4f(uint16_t first, uint16_t second); /* 0x55760 AX/DX */
void send_two_word_byte_packet_42(uint16_t first, uint16_t second, uint8_t flag); /* 0x55bd0 AX/DX/BL */
void clear_scene_word_latches(void); /* 0x56004 */
void send_dword_marker_packet_2a(uint32_t value); /* 0x561bc EAX */
void send_marker_word_packet_30(void); /* 0x56d40 */
void clear_activity_record_flag(ef_addr32 record); /* 0x55860 EAX */
void forward_marker_matched_record(ef_addr32 record); /* 0x561e4 EAX */
void notify_if_scene_pair(ef_addr32 record); /* 0x56954 EAX */
void guest_refresh_activity_records(void); /* 0x544c0 */
void guest_apply_marker_update(uint32_t value, uint8_t flag); /* 0x562d8 EAX/DL */
uint32_t guest_query_scene_pair(uint8_t selector, int32_t high); /* 0x87f9c EAX/EDX */
void guest_notify_scene_pair(void); /* 0x878c8 */

void send_byte_word_byte_packet_3e(uint8_t first, uint16_t second, uint8_t third); /* 0x571f0 AL/DX/BL */
void send_two_word_byte_packet_3b(uint16_t first, uint16_t second, uint8_t flag); /* 0x5728c AX/DX/BL */
void send_word_byte_packet_3d(uint16_t value, uint8_t flag); /* 0x57348 AX/DL */
void send_two_word_packet_40(uint16_t first, uint16_t second); /* 0x575fc AX/DX */
void send_word_packet_2e(uint16_t value); /* 0x57650 AX */
void bump_counter_19b960(ef_addr32 record); /* 0x576a4 EAX */
void bump_counter_19b96c(ef_addr32 record); /* 0x576d4 EAX */
void bump_counter_19b966(ef_addr32 record); /* 0x57704 EAX */
void set_mode_limits_4_5(void); /* 0x58d04 */
void set_mode_limits_8_10(void); /* 0x58d44 */

void apply_runtime_word_forward(void); /* 0x6454c */
void apply_runtime_word_negated(void); /* 0x64524 */
void load_halfword_pair(void); /* 0x68bf4 */
void load_word_0x1282dc(void); /* 0x69080 */
uint32_t lookup_record_value(uint8_t index); /* 0x6a0f8 AL -> EAX */
uint8_t record_mode_is_four(uint8_t index); /* 0x6b420 AL -> AL */
uint32_t lookup_table_by_quotient(void); /* 0x77afc -> EAX */
void reset_pair_counter(void); /* 0x7772c */
void guest_runtime_acquire(ef_addr32 object); /* 0xd0b68 EAX */
void guest_runtime_set(ef_addr32 object, uint32_t value); /* 0xd0ebc EAX/EDX */
uint16_t guest_load_halfword(ef_addr32 source); /* 0xde864 EAX -> AX */
uint32_t guest_lookup_value(ef_addr32 source); /* 0x9ddfc EAX -> EAX */
uint16_t guest_read_selector_word(void); /* 0xa45b3 -> AX */

/* Status value formatters and small state helpers. */
void write_status_byte_1fcb30(ef_addr32 destination); /* 0x82720 EAX */
void write_status_byte_1a5c0d(ef_addr32 destination); /* 0x8273c EAX */
void write_status_high_word(ef_addr32 destination); /* 0x82758 EAX */
void write_status_byte_15b7e4(ef_addr32 destination); /* 0x82774 EAX */
void write_status_queried_value(ef_addr32 destination); /* 0x827b4 EAX */
void write_status_dword_122dc4(ef_addr32 destination); /* 0x827f4 EAX */
void write_status_byte_15bdd0(ef_addr32 destination); /* 0x82bf0 EAX */
uint8_t set_pending_mode_two(void); /* 0x845a4 -> AL */
void load_halfword_1fcaec(void); /* 0x81794 */
void release_block_12afc4(void); /* 0x7a434 */
void format_ui_status_value(ef_addr32 destination, ef_addr32 format, uint32_t value); /* 0xa4342 */
uint32_t guest_query_status_value(ef_addr32 destination); /* 0x810ec EAX -> EAX */
void guest_release_block(ef_addr32 block); /* 0xc3f80 EAX */

/* misc_state.c: small state helpers. */
void decrement_grid_row_link(int16_t cell); /* 0x85400 AX */
void clear_input_state_block(void); /* 0x8828c */
void adjust_record_scalar(ef_addr32 record); /* 0x89140 EAX */
void clear_dword_12db5c(void); /* 0x8ba78 */
void copy_dword_20f420_to_132ad4(void); /* 0x8bdd4 */
void clear_matching_link_a(ef_addr32 record, uint32_t value); /* 0x8f5d8 EAX/EDX */
void clear_matching_link_b(ef_addr32 record, uint32_t value); /* 0x90924 EAX/EDX */
void begin_mode_35(void); /* 0x91238 */
void end_mode_36(void); /* 0x919b8 */
uint32_t length_squared_word_triple(int16_t x, int16_t y, int16_t z); /* 0x91c00 AX/DX/BX -> EAX */
uint8_t scene_words_differ(void); /* 0x91c34 -> AL */
uint32_t forward_record_field_30(ef_addr32 record); /* 0x94948 EAX -> EAX */
void reset_scene_words_and_update(void); /* 0x94ce8 */
void select_scene_object(void); /* 0x94ffc */
void reset_record_range(ef_addr32 record, uint32_t count); /* 0x9783c EAX/EDX */
uint32_t sample_tick_counter(void); /* 0x97dc0 -> EAX */
uint32_t pointer_or_default(void); /* 0x97f70 -> EAX */
void forward_if_enabled(void); /* 0x98000 */
void install_allocator_hooks(void); /* 0x98558 */
void guest_after_scalar_adjust(ef_addr32 record); /* 0x89924 EAX */
uint8_t guest_link_check_a(ef_addr32 record, uint32_t value); /* 0x8c09c EAX/EDX -> AL */
uint8_t guest_link_check_b(ef_addr32 record, uint32_t value); /* 0x90050 EAX/EDX -> AL */
void guest_mode_36_hook(uint32_t first, uint32_t second); /* 0xdc334 stack, callee cleanup */
uint32_t guest_square_root_of(int32_t value); /* 0xe2b60 EAX -> EAX */
uint32_t guest_record_field_30(ef_addr32 record, uint32_t field); /* 0x9457c EAX/EDX -> EAX */
void guest_scene_update(void); /* 0x94a74 */
void guest_scene_apply(void); /* 0x94d08 */
void guest_record_tail(ef_addr32 record); /* 0x975f0 EAX */
void guest_tick_sample(void); /* 0x10640 */
uint32_t guest_current_pointer(void); /* 0x9c858 -> EAX */
void guest_forward_value(uint32_t value); /* 0xd5f0c EAX */

int32_t random_modulo(int32_t divisor); /* 0xf60 EAX -> EAX */
int32_t max3_signed(int32_t a, int32_t b, int32_t c); /* 0x3cd4 EAX/EDX/EBX */
void clear_command_state_6bf4(void); /* 0x6bf4 */
uint8_t record_value_not_above_limit(ef_addr32 record); /* 0x73d4 EDX -> AL */
void latch_tick_counter(void); /* 0x1064c */
uint32_t elapsed_ticks(void); /* 0x10688 -> EAX */
void set_runtime_slot_flag(int16_t index, uint8_t value); /* 0x11d20 AX/DL */
void refresh_all_runtime_slots(void); /* 0x12140 */
uint8_t read_event_slot_byte(int16_t index); /* 0x14d3c AX -> AL */
void reset_dialog_latches(void); /* 0x151a0 */
uint32_t flag_object_and_forward(int16_t index); /* 0x17388 AX */
void decrement_grid_row_link_wide(int32_t cell); /* 0x173a4 EAX */
void store_dword_pair_19b430(uint32_t first, uint32_t second); /* 0x27538 EAX/EDX */
uint8_t filtered_object_has_link(int16_t index); /* 0x28a70 AX -> AL */
uint8_t object_has_link(int16_t index); /* 0x28a8c AX -> AL */
uint8_t point_range_contains_limits(int32_t low_x, int32_t low_y, int32_t high_x, int32_t high_y); /* 0x2a4a0 EAX/EDX/EBX/ECX */
void clear_dwords_19bb38(void); /* 0x2a794 */
uint32_t call_32c04_182ad8(void); /* 0x32b3c */
uint32_t call_32c04_182adc(void); /* 0x32b48 */
uint32_t call_32c04_182ae0(void); /* 0x32b54 */
uint32_t call_32c04_182ae4(void); /* 0x32b60 */
uint32_t call_32c04_182ae8(void); /* 0x32b6c */
uint32_t call_32c04_182af8(void); /* 0x32b9c */
uint32_t call_32c04_182afc(void); /* 0x32ba8 */
void forward_if_flag_1357c8(void); /* 0x33750 */
void install_callback_53ecc(void); /* 0x3918c */
uint8_t grid_row_state_allowed(int16_t cell); /* 0x3bd28 AX -> AL */
uint32_t forward_value_1a2e78(void); /* 0x3c434 */
void copy_table_entry_to_record(int16_t index); /* 0x3c768 AX */
uint32_t activate_object_3cc94(void); /* 0x3cd24 */
uint32_t activate_object_3cb2c(void); /* 0x3cd40 */
void fill_random_offsets(void); /* 0x3ce04 */
void run_reset_sequence_3dcb0(void); /* 0x3dcb0 */
void reset_world_state_410d0(void); /* 0x410d0 */
uint32_t code_is_0b_or_0c(uint8_t code); /* 0x41df4 AL -> EAX */
uint32_t code_is_26_or_27(uint8_t code); /* 0x41e0c AL -> EAX */
uint32_t code_is_0_16_or_2(uint8_t code); /* 0x41e84 AL -> EAX */
/* Guest dependencies for low_helpers.c. */
uint32_t guest_random_word(void); /* 0x9c960 -> EAX */
ef_addr32 guest_lookup_object_8ffe8(int32_t index); /* 0x8ffe8 EAX -> EAX */
uint32_t guest_forward_high_word(int32_t value); /* 0x3739c EAX tail */
uint32_t guest_fn_32c04(uint32_t table); /* 0x32c04 EAX tail */
void guest_fn_a41c8(void); /* 0xa41c8 */
uint32_t guest_fn_e2af4(uint32_t value); /* 0xe2af4 EAX */
ef_addr32 guest_table_entry_3c730(ef_addr32 table, int32_t index); /* 0x3c730 EAX/EDX -> EAX */
uint32_t guest_fn_3ccec(void); /* 0x3ccec -> EAX */
void guest_fn_3cc94(void); /* 0x3cc94 */
void guest_fn_3cb2c(void); /* 0x3cb2c */
uint32_t guest_fn_e2fdb(void); /* 0xe2fdb -> EAX */
void guest_fn_394e0(void); /* 0x394e0 */
void guest_fn_8846c(void); /* 0x8846c */
void guest_fn_888cc(void); /* 0x888cc tail */
void guest_fn_53270(void); /* 0x53270 */
void guest_fn_8bd58(ef_addr32 first, ef_addr32 second); /* 0x8bd58 EAX/EDX */
void guest_fn_49228(void); /* 0x49228 */

/* mid_helpers.c */
uint32_t code_is_0_1_5_6_7(uint8_t code); /* 0x41ea0 AL -> EAX */
uint32_t code_is_2_3_18_19(uint8_t code); /* 0x41f00 AL -> EAX */
void reset_pointer_126688(void); /* 0x47ee4 */
void sync_scene_marker(void); /* 0x47f9c */
void transform_string_1a5968(void); /* 0x50d9c */
void mark_filtered_object(ef_addr32 record); /* 0x55084 EAX */
void forward_record_to_46454(ef_addr32 record); /* 0x5511c EAX */
void set_activity_record_flag(ef_addr32 record); /* 0x55884 EAX */
void dispatch_event_triple(ef_addr32 record); /* 0x56928 EAX */
void send_marker_word_packet_4e(void); /* 0x56c90 */
void send_marker_word_packet_33(void); /* 0x56ed0 */
void clear_flag_bit_1a662e(ef_addr32 record); /* 0x56d68 EAX */
void copy_record_words_12583c(ef_addr32 record); /* 0x56fa8 EAX */
void flag_marker_zero(void); /* 0x56fe8 */
uint32_t forward_filtered_object(ef_addr32 record); /* 0x57670 EAX */
void notify_selected_object(void); /* 0x5f8a8 */
uint32_t record_field_16_or_default(ef_addr32 record); /* 0x65498 EAX */
uint32_t forward_object_level(void); /* 0x70418 */
uint32_t nonzero_or_one(uint32_t value); /* 0x757b0 EAX */
uint32_t lookup_by_word_checked(uint16_t word); /* 0x77950 DX */
uint32_t lookup_by_word(int16_t word); /* 0x80d20 DX */
uint32_t random_table_77b20(void); /* 0x77b20 */
uint32_t random_table_77b44(void); /* 0x77b44 */
uint32_t random_table_77b68(void); /* 0x77b68 */
uint32_t random_table_77b8c(void); /* 0x77b8c */
uint32_t random_table_77bb0(void); /* 0x77bb0 */
void copy_and_sync_scene_marker(void); /* 0x77c18 */
uint32_t call_79ff0_12af60(void); /* 0x7a298 */
void reset_sound_state_7a454(void); /* 0x7a454 */
uint32_t forward_value_1f8e90(void); /* 0x7a510 */
uint32_t call_b1a10_1379bc(void); /* 0x7ccd0 */
uint8_t scene_slot_valid(void); /* 0x7e89c -> AL */
uint32_t record_mismatches_marker(ef_addr32 record); /* 0x7f6ac EAX */
void wait_for_event_803e8(void); /* 0x803e8 */
void guest_fn_46070(void); /* 0x46070 */
uint32_t guest_fn_e7835(uint32_t value); /* 0xe7835 EAX */
void guest_fn_46454(int32_t a, int32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f); /* 0x46454 EAX/EDX/EBX/ECX + 2 stack, callee cleanup */
void guest_dispatch_event_triple(const uint32_t *triple, int32_t high_word); /* 0x1d5f4 EAX/EDX */
void guest_fn_a3f30(void); /* 0xa3f30 */
uint32_t guest_fn_741f0(ef_addr32 object); /* 0x741f0 EAX tail */
void guest_fn_9e508(int32_t value, uint32_t selector); /* 0x9e508 EAX/EDX */
uint32_t guest_fn_6e390(uint32_t level, uint32_t mode); /* 0x6e390 EAX/EDX */
uint32_t guest_fn_a1a40(void); /* 0xa1a40 -> EAX */
uint32_t guest_fn_a1a90(int32_t word, uint32_t base); /* 0xa1a90 EAX/EDX */
uint32_t guest_fn_79ff0(uint32_t value); /* 0x79ff0 EAX tail */
void guest_fn_7a9bc(void); /* 0x7a9bc */
uint32_t guest_fn_b1a10(uint32_t value); /* 0xb1a10 EAX tail */
uint32_t guest_fn_2c358(int32_t slot); /* 0x2c358 EAX -> AL */
uint32_t guest_fn_d7594(uint32_t value); /* 0xd7594 EAX -> AL */

/* scene_helpers.c */
uint32_t forward_if_flag_12b104(uint8_t value); /* 0x815f4 AL */
void scale_vector_down_16(ef_addr32 vector); /* 0x83734 EAX */
void forward_vector_to_83708(ef_addr32 vector); /* 0x84394 EAX */
uint8_t reset_pending_mode_844e4(void); /* 0x844e4 -> AL */
void reset_target_search(void); /* 0x88548 */
void copy_24_bytes(ef_addr32 destination, ef_addr32 source); /* 0x88d08 EAX/EDX */
void finish_record_89960(ef_addr32 record); /* 0x89960 EAX */
void copy_selected_object(void); /* 0x8b530 */
void select_object_8cd48(void); /* 0x8cd48 */
void clear_blocks_8fe30(void); /* 0x8fe30 */
void reset_scene_update_94194(void); /* 0x94194 */
void reset_scene_update_941bc(void); /* 0x941bc */
void update_record_978e8(ef_addr32 record); /* 0x978e8 EAX */
void draw_pair_97d90(int16_t x, int16_t y, uint32_t extra); /* 0x97d90 AX/DX/EBX */
void reset_sound_state_984b4(void); /* 0x984b4 */
void forward_sound_state_982d4(void); /* 0x982d4 */
uint8_t record_flag_bit4(int16_t index); /* 0xa0e98 AX -> AL */
void set_record_words(int16_t index, uint16_t first, uint16_t second, uint16_t third); /* 0xa0ea8 AX/DX/BX/CX */
uint8_t record_flag_bits56(int16_t index); /* 0xa1858 AX -> AL */
ef_addr32 record_entry_address(int16_t index); /* 0xa1a10 AX -> EAX */
void clear_entry_a2e4c(uint16_t index); /* 0xa2e4c AX */
void clear_all_entries_a2e6c(void); /* 0xa2e6c */
void format_entry_a2e90(uint16_t index); /* 0xa2e90 AX */
uint32_t forward_if_clear_a3108(uint16_t first, uint16_t second); /* 0xa3108 AX/DX */
void clear_tables_a38c8(void); /* 0xa38c8 */
uint8_t string_short_enough(ef_addr32 text); /* 0xa3a10 EAX -> AL */
void fill_block_a3b48(void); /* 0xa3b48 */
void init_entry_table_a3bfc(void); /* 0xa3bfc */
void clear_dwords_135820(void); /* 0xa4318 */
ef_addr32 uppercase_string(ef_addr32 text); /* 0xa4421 EAX -> EAX */
void refresh_widget_a4b5c(ef_addr32 widget); /* 0xa4b5c EAX */
void refresh_all_widgets_a4c84(void); /* 0xa4c84 */
void clear_widget_table_a4e98(void); /* 0xa4e98 */
void init_widget_margins(ef_addr32 widget); /* 0xa4ec0 EAX */
void update_widget_child_a51dc(ef_addr32 widget); /* 0xa51dc EAX */
void forward_widget_position(ef_addr32 widget); /* 0xa54f0 EAX */
void clear_widget_words_a56f8(ef_addr32 widget); /* 0xa56f8 EAX */
void update_all_widgets_a5980(void); /* 0xa5980 */
void clear_child_words_a59ac(ef_addr32 widget); /* 0xa59ac EAX */
void walk_children_a66a0(ef_addr32 parent); /* 0xa66a0 EAX */
void walk_children_a6a64(ef_addr32 parent); /* 0xa6a64 EAX */
void clear_widget_dirty(ef_addr32 widget); /* 0xa7380 EAX */
void set_widget_bit6(ef_addr32 widget, uint8_t value); /* 0xa739c EAX/DL */
void set_children_bit6(ef_addr32 parent, uint8_t value); /* 0xa7408 EAX/DL */
void clear_children_dirty(ef_addr32 parent); /* 0xa7434 EAX */
uint32_t call_9c858_18d57c(void); /* 0xa7b80 */
void latch_b3f48(void); /* 0xa7b8c */
uint32_t guest_fn_d72b4(uint32_t value); /* 0xd72b4 EAX tail */
void guest_fn_e99b8(ef_addr32 vector); /* 0xe99b8 EAX */
void guest_fn_83708(ef_addr32 destination, uint32_t x, uint32_t y, uint32_t z); /* 0x83708 EAX/EDX/EBX/ECX */
void guest_fn_dc832(uint32_t first, uint32_t second, uint32_t count); /* 0xdc832 EAX/EDX/EBX */
void guest_fn_8a318(ef_addr32 record); /* 0x8a318 EAX */
uint32_t guest_fn_8c0f0(uint32_t value); /* 0x8c0f0 EAX -> EAX */
ef_addr32 guest_fn_8c0bc(int32_t value); /* 0x8c0bc EAX -> EAX */
void guest_fn_b3fb0(uint32_t destination, uint32_t value, uint32_t count); /* 0xb3fb0 EAX/EDX/EBX */
void guest_fn_91df4(void); /* 0x91df4 */
void guest_fn_9202c(void); /* 0x9202c */
void guest_fn_93a8c(void); /* 0x93a8c */
void guest_fn_979c8(ef_addr32 record); /* 0x979c8 EAX */
void guest_fn_97914(ef_addr32 record); /* 0x97914 EAX */
void guest_fn_97950(ef_addr32 record); /* 0x97950 EAX */
void guest_fn_e30e0(uint32_t mode, int32_t x, int32_t y, uint32_t extra); /* 0xe30e0 EAX/EDX/EBX/ECX */
void guest_fn_de4b0(void); /* 0xde4b0 */
void guest_fn_de3b4(int32_t value); /* 0xde3b4 EAX tail */
ef_addr32 guest_fn_b3f58(int32_t index); /* 0xb3f58 EAX -> EAX */
void guest_fn_e9a24(uint32_t a, uint32_t b, uint32_t c); /* 0xe9a24 stack, callee cleanup */
uint32_t guest_fn_ec0ec(uint32_t first, uint32_t second); /* 0xec0ec EAX/EDX tail */
uint32_t guest_fn_b1b8b(ef_addr32 text); /* 0xb1b8b EAX -> EAX */
void guest_fn_deb87(uint32_t destination, uint32_t value, uint32_t count); /* 0xdeb87 EAX/EDX/ECX */
void guest_fn_a4a94(uint32_t value); /* 0xa4a94 EAX */
void guest_fn_a4a20(uint32_t value); /* 0xa4a20 EAX */
void guest_fn_a6038(void); /* 0xa6038 */
void guest_fn_a4b80(uint32_t index); /* 0xa4b80 EAX */
void guest_fn_a5484(ef_addr32 widget, int32_t x, int32_t y); /* 0xa5484 EAX/EDX/EBX */
void guest_fn_f13ac(ef_addr32 widget); /* 0xf13ac EAX tail */
void guest_fn_a5764(uint32_t value); /* 0xa5764 EAX */
ef_addr32 guest_fn_a788c(ef_addr32 widget, ef_addr32 name); /* 0xa788c EAX/EDX -> EAX */
void guest_fn_a6438(ef_addr32 parent, ef_addr32 child); /* 0xa6438 EAX/EDX */
void guest_fn_a69d0(ef_addr32 parent, ef_addr32 child); /* 0xa69d0 EAX/EDX */
void guest_fn_a6d00(ef_addr32 widget); /* 0xa6d00 EAX */
void guest_fn_a5ef8(uint32_t value); /* 0xa5ef8 EAX */
uint32_t guest_fn_9c858_arg(uint32_t value); /* 0x9c858 EAX tail */
uint32_t guest_fn_b3f48(void); /* 0xb3f48 -> EAX */

/* list_helpers.c */
void add_to_counter_4191c8(uint32_t delta);
void latch_b3f50(void);
void add_to_counter_4191c4(uint32_t delta);
void set_latches_1371fc(void);
void save_table_418e38(void);
void restore_table_418e38(void);
void clamp_word_425fa6(void);
void pop_table_entry_4191dc(void);
int32_t random_scaled(int16_t scale);
void save_state_a98d0(void);
void restore_state_a9900(void);
uint32_t restore_state_a9990(void);
void clear_latch_if_idle(void);
void request_by_state(void);
void clear_latch_and_notify(void);
void resolve_link_at_1(ef_addr32 record);
void resolve_link_at_0(ef_addr32 record);
ef_addr32 walk_chain_from_8(int16_t steps, ef_addr32 record);
void emit_record_pair_ab0b0(ef_addr32 target, ef_addr32 record);
void emit_record_pair_ab260(ef_addr32 target, ef_addr32 record);
void latch_c382c(void);
void clear_pool_ab8c4(void);
void init_pool_ab8e0(uint32_t size);
void clear_block_abe24(void);
void set_eight_pointers(uint32_t value);
void set_view_words_and_update(uint16_t a, uint16_t b, uint16_t c, uint16_t d);
void reset_view_ac9e8(void);
void clear_word_41eeda(void);
void set_word_41eeda(void);
void clear_words_13799e(void);
void free_list_b1814(ef_addr32 head, ef_addr32 tail);
void free_list_b188c(ef_addr32 head, ef_addr32 tail);
void free_list_b1974(ef_addr32 head, ef_addr32 tail);
void free_list_b19e4(ef_addr32 head, ef_addr32 tail);
void free_list_b1ae4(ef_addr32 head, ef_addr32 tail);
void release_pointer_b1a10(ef_addr32 slot);
void release_pointer_b1a3c(ef_addr32 slot);
uint32_t count_active_entries(ef_addr32 record);
uint32_t latch_and_forward_b33ac(void);
int32_t find_free_entry(int16_t start, ef_addr32 record);
int32_t divisor_for_rate(int16_t rate);
int32_t random_modulo_high_word(ef_addr32 record);
void cross_product_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b);
void add_vectors_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b);
void sub_vectors_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b);
uint32_t guest_fn_b3f50(void); /* 0xb3f50 -> EAX */
void guest_fn_f41dc(void); /* 0xf41dc */
void guest_fn_a7a1c(uint32_t a, uint32_t b, uint32_t c); /* 0xa7a1c stack, callee cleanup */
void guest_fn_a79d4(uint32_t a, uint32_t b, uint32_t c); /* 0xa79d4 stack, callee cleanup */
uint32_t guest_fn_c382c(void); /* 0xc382c -> EAX */
uint32_t guest_fn_e29e8(uint32_t size); /* 0xe29e8 EAX -> EAX */
void guest_fn_ac7f0(void); /* 0xac7f0 tail */
void guest_fn_aca14(void); /* 0xaca14 */
void guest_fn_b17d4(ef_addr32 node); /* 0xb17d4 EAX */
void guest_fn_b1840(ef_addr32 node); /* 0xb1840 EAX */
void guest_fn_b1918(ef_addr32 node); /* 0xb1918 EAX */
void guest_fn_b19a0(ef_addr32 node); /* 0xb19a0 EAX */
void guest_fn_b1a68(ef_addr32 node); /* 0xb1a68 EAX */
void guest_fn_cc768(ef_addr32 pointer); /* 0xcc768 EAX */
uint32_t guest_fn_a4574(uint32_t value); /* 0xa4574 EAX -> EAX */
uint32_t guest_fn_f9ecd(uint32_t value); /* 0xf9ecd EAX tail */

/* list_lookup.c */
ef_addr32 find_node_by_key(ef_addr32 head, ef_addr32 record, uint16_t word, uint8_t byte);
void fill_ten_bytes_04(ef_addr32 destination);
void fill_ten_bytes_0d(ef_addr32 destination);
void fill_pair_by_flag(ef_addr32 destination, uint8_t flag);
void fill_four_bytes_2233(ef_addr32 destination);
void run_if_available_bc34f(ef_addr32 argument);
void run_if_available_bc36a(ef_addr32 argument);
int32_t digit_character(int32_t value);
void list_insert_head(ef_addr32 node, ef_addr32 head, ef_addr32 tail);
uint32_t list_count(ef_addr32 head);
ef_addr32 list_nth(ef_addr32 head, uint32_t index);
uint32_t record_is_strong(ef_addr32 record);
uint32_t record_is_elite(ef_addr32 record);
uint8_t map_code_bcc04(uint8_t code);
uint8_t map_code_bcd78(uint8_t code);
uint8_t map_code_bce98(uint8_t code);
uint32_t record_flag_clear_d(ef_addr32 record);
uint32_t records_share_position(ef_addr32 first, ef_addr32 second);
uint32_t code_is_2_3_4_9_0c(uint8_t code);
uint32_t value_below_word_23(ef_addr32 record, uint8_t value);
uint32_t count_nonzero_entries(ef_addr32 record);
uint32_t record_pending_becf8(ef_addr32 record);
ef_addr32 find_node_by_pair(int16_t first, int16_t second, ef_addr32 head);
uint32_t linked_records_differ(ef_addr32 record);
ef_addr32 find_node_by_record_a(ef_addr32 record, ef_addr32 head);
ef_addr32 find_node_by_record_b(ef_addr32 head, ef_addr32 record);
ef_addr32 find_node_by_word_c1b8c(uint16_t key, ef_addr32 head);
ef_addr32 find_node_by_word_cb5f8(uint16_t key, ef_addr32 head);
ef_addr32 list_nth_word(uint16_t index, ef_addr32 head);
void call_e9a24_c3834(void);
void call_e9a24_c3850(void);
void call_e9a24_c386c(void);
void call_e9a24_c3888(void);
void clear_buffer_c38a4(void);
void select_buffer_146e40(void);
void select_buffer_146e44(void);
void select_buffer_15b758(void);
void select_buffer_15b758_both(void);
void swap_buffers_c3a28(void);
void release_buffer_c3a48(void);
uint8_t read_buffer_byte(uint32_t offset, uint32_t row);
void run_e3c20(void);
int32_t grid_index_ca9fc(int32_t x, int32_t y);
uint32_t guest_fn_fa6d3(uint32_t value); /* 0xfa6d3 EAX -> EAX */
void guest_fn_bc287(ef_addr32 argument); /* 0xbc287 EAX */
uint32_t guest_fn_bc7ec(ef_addr32 record); /* 0xbc7ec EAX -> AX */
uint32_t guest_fn_becc0(ef_addr32 record); /* 0xbecc0 EAX -> AX */
void guest_fn_c3f80(uint32_t buffer); /* 0xc3f80 EAX */
void guest_fn_e3c20(void); /* 0xe3c20 */

/* code_predicates.c */
uint8_t code_if_one_to_four(uint8_t code);
void release_pointer_1379c8(void);
void mark_list_entry_9c(uint8_t node_index, uint8_t slot, ef_addr32 head);
void mark_list_entry_64(uint8_t node_index, uint8_t slot, ef_addr32 head);
ef_addr32 find_node_by_byte_b(uint8_t key, ef_addr32 head);
uint32_t code_is_10_to_15(uint8_t code);
uint32_t code_is_18_to_1b_1d_1e(uint8_t code);
uint32_t code_is_18_or_1c_to_1f(uint8_t code);
uint32_t code_is_20_or_21(uint8_t code);
uint32_t code_is_23_or_24(uint8_t code);
uint32_t code_is_22(uint8_t code);
uint32_t pair_matches_cc7e8(uint8_t value, uint8_t kind);
uint32_t pair_matches_cc81c(uint8_t value, uint8_t kind);
uint16_t cursor_word_at_2(void);
uint16_t cursor_table_word(void);
int32_t cursor_table_address(void);
uint32_t advance_cursor(void);
void reset_handler_cd6fc(void);
void store_four_dwords_17cf9c(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void release_buffer_cdc38(void);
void counted_array_append(ef_addr32 array, uint32_t value);
int32_t forward_triple_cff64(ef_addr32 record);
uint32_t forward_pair_d13dc(ef_addr32 record);
int32_t angle_d13ac(ef_addr32 record);
int32_t angle_d13ec(ef_addr32 record);
void call_a7990_420944(void);
void copy_four_dwords_15b87c(void);
void forward_record_d2110(ef_addr32 record);
void call_103778_if_enabled(uint32_t first, uint32_t second);
uint32_t sum_three_bytes(ef_addr32 bytes);
uint8_t store_handle_if_enabled(ef_addr32 destination);
void process_string_list(ef_addr32 list);
uint8_t device_state_is_2(uint8_t index);
uint8_t device_state_is_4(uint8_t index);
int32_t add_to_word_11(ef_addr32 record, uint16_t delta);
void guest_fn_9c3cf(uint32_t handler); /* 0x9c3cf EAX */
void guest_fn_c37ec(void); /* 0xc37ec */
uint32_t guest_fn_b3830(int32_t a, int32_t b, int32_t c); /* 0xb3830 EAX/EDX/EBX -> AX */
uint32_t guest_fn_b3b1c(uint32_t a, uint32_t b); /* 0xb3b1c EAX/EDX -> EAX */
void guest_fn_a7990(uint32_t value); /* 0xa7990 stack, callee cleanup */
void guest_fn_fc8cc(uint32_t first, uint32_t second); /* 0xfc8cc stack, callee cleanup */
void guest_fn_103778(uint32_t a, uint32_t b, uint32_t c); /* 0x103778 cdecl */
void guest_fn_d6fa8(uint32_t text); /* 0xd6fa8 EAX */
uint32_t guest_fn_1028f4(uint32_t handle); /* 0x1028f4 cdecl -> EAX */

/* math_string.c */
void clear_buffer_dc2c0(void);
void update_view_scale_dc314(void);
void halve_pair_to_fit16(ef_addr32 first, ef_addr32 second);
int32_t to_upper_ascii(int32_t c);
int32_t to_lower_ascii(int32_t c);
void plot_byte(uint8_t value, uint32_t x, uint32_t y);
void plot_byte_clipped(uint8_t value, int32_t x, int32_t y);
uint32_t length_squared_dec90(int16_t x, int16_t y, int16_t z);
uint32_t length_2d_decb0(int32_t x, int32_t y);
uint32_t length_3d_decc4(int32_t x, int32_t y, int32_t z);
uint32_t integer_sqrt(uint32_t value);
uint32_t random_lcg(void);
void advance_counter_15f07d(void);
void reset_table_15f5fd(void);
uint8_t next_enabled_index(uint8_t index);
void clear_block_e17a0(void);
uint8_t read_table_e192c(uint8_t index);
void wait_for_counter_15f748(int32_t count);
void reset_buffer_e2a1c(void);
uint8_t flag_is_one_15f930(void);
void copy_record_words_e599c(ef_addr32 record);
void apply_record_e5870(ef_addr32 record);
void apply_record_e588c(ef_addr32 record);
ef_addr32 call_e67a0(ef_addr32 object, int16_t x, int16_t y, int16_t z);
void adjust_list_e6818(ef_addr32 head);
void install_handler_e7fa4(uint32_t value);
void rotate_calls_e8348(uint32_t a, uint32_t b, uint32_t c);
void store_bytes_46437d(uint8_t first, uint8_t second);
ef_addr32 allocate_cleared(uint32_t count, uint32_t size);
ef_addr32 find_char(ef_addr32 text, uint32_t ch);
uint32_t strip_carriage_returns(ef_addr32 text);
ef_addr32 split_line(ef_addr32 text);
int32_t compare_dwords(ef_addr32 first, ef_addr32 second);
void release_record_ea240(ef_addr32 record);
void clear_vector3(ef_addr32 vector);
void copy_vector3(ef_addr32 destination, ef_addr32 source);
void add_vector3(ef_addr32 destination, ef_addr32 source);
void set_vector3_from_words(ef_addr32 vector, int16_t x, int16_t y, int16_t z);
int32_t dot_product_fixed(ef_addr32 first, ef_addr32 second);
ef_addr32 lowercase_string(ef_addr32 text);
void release_pointer_eb4c0(ef_addr32 slot);
void guest_fn_dc3d5(void); /* 0xdc3d5 */
void guest_fn_ce354(void); /* 0xce354 */
ef_addr32 guest_fn_e2fd5(void); /* 0xe2fd5 -> EAX */
void guest_fn_e52a4(uint32_t value, uint32_t mode); /* 0xe52a4 EAX/EDX */
void guest_fn_e5960(ef_addr32 record); /* 0xe5960 EAX */
void guest_fn_e67a0(ef_addr32 object, int32_t x, int32_t y, int32_t z); /* 0xe67a0 EAX/EDX/EBX/ECX */
void guest_fn_e81b0(uint32_t a, uint32_t b); /* 0xe81b0 EAX/EDX */
ef_addr32 guest_fn_fc795(uint32_t size); /* 0xfc795 EAX -> EAX */

/* misc_helpers2.c */
uint32_t lookup_field_eba4c(uint32_t key, uint32_t index);
void release_or_forward_ebcd8(uint32_t flags, ef_addr32 object);
uint32_t advance_cursor_ebcfc(uint32_t amount, uint32_t flags, ef_addr32 object);
void store_through_ebd30(uint32_t value);
void store_through_ebd36(uint32_t value);
uint8_t compare_names_ec0ec(uint16_t first, uint16_t second);
void reset_table_ece90(void);
int32_t guest_memcmp(ef_addr32 first, ef_addr32 second, uint32_t count);
void uppercase_string_edb15(ef_addr32 text);
void process_words_f17d8(ef_addr32 record);
void process_words_f180c(ef_addr32 record);
int32_t file_position_f227f(ef_addr32 file);
void draw_text_f23ac(uint8_t color, int32_t x, int32_t y, ef_addr32 text);
uint32_t result_f2800(void);
uint8_t path_ends_with_colon(void);
void strip_extension(ef_addr32 text);
void copy_path_f2e54(void);
void refresh_f3480(void);
void refresh_f34a0(void);
void load_path_value_f35d8(void);
void refresh_f35f0(void);
void load_path_f3b1c(void);
void load_path_value_f3b40(void);
void load_path_pointer_f3b58(void);
void reset_flags_f4178(void);
void call_f9dcb(uint32_t value);
void store_two_digits(int32_t value, ef_addr32 base, int32_t offset);
void update_flags_fa7d2(ef_addr32 file);
void call_fba6c(uint32_t first, ef_addr32 record);
uint32_t map_address_fddd2(uint32_t address);
void fill_table_fef26(void);
void update_fc9f8(int32_t value);
void trace_call_101f44(uint32_t a);
void trace_call_101ff4(uint32_t a);
void trace_call_1022dc(uint32_t a);
void trace_call_102330(uint32_t a);
void trace_call_102384(uint32_t a);
void trace_call_102488(uint32_t a);
void trace_call_102644(uint32_t a);
void trace_call_102670(uint32_t a);
void trace_call_102774(uint32_t a);
void trace_call_1027a0(uint32_t a);
void trace_call_1027cc(uint32_t a);
void trace_call_1027f8(uint32_t a);
void trace_call_10311c(uint32_t a);
void trace_call_103148(uint32_t a);
void trace_call_1031a0(uint32_t a);
void trace_call_101ed4(uint32_t a, uint32_t b);
void trace_call_1021f4(uint32_t a, uint32_t b);
void trace_call_102228(uint32_t a, uint32_t b);
void trace_call_10225c(uint32_t a, uint32_t b);
void trace_call_102824(uint32_t a, uint32_t b);
void trace_call_102858(uint32_t a, uint32_t b);
void trace_call_10288c(uint32_t a, uint32_t b);
void trace_call_1028c0(uint32_t a, uint32_t b);
void trace_call_102a98(uint32_t a, uint32_t b);
void guest_fn_1111ca(uint32_t a); /* 0x1111ca cdecl */
void guest_fn_1112ed(uint32_t a); /* 0x1112ed cdecl */
void guest_fn_11141f(uint32_t a); /* 0x11141f cdecl */
void guest_fn_111473(uint32_t a); /* 0x111473 cdecl */
void guest_fn_1113d4(uint32_t a); /* 0x1113d4 cdecl */
void guest_fn_104464(uint32_t a); /* 0x104464 cdecl */
void guest_fn_106634(uint32_t a); /* 0x106634 cdecl */
void guest_fn_106644(uint32_t a); /* 0x106644 cdecl */
void guest_fn_10683c(uint32_t a); /* 0x10683c cdecl */
void guest_fn_10687c(uint32_t a); /* 0x10687c cdecl */
void guest_fn_106894(uint32_t a); /* 0x106894 cdecl */
void guest_fn_1068b8(uint32_t a); /* 0x1068b8 cdecl */
void guest_fn_11402c(uint32_t a); /* 0x11402c cdecl */
void guest_fn_114058(uint32_t a); /* 0x114058 cdecl */
void guest_fn_114170(uint32_t a); /* 0x114170 cdecl */
void guest_fn_111142(uint32_t a, uint32_t b); /* 0x111142 cdecl */
void guest_fn_1114c7(uint32_t a, uint32_t b); /* 0x1114c7 cdecl */
void guest_fn_1114f7(uint32_t a, uint32_t b); /* 0x1114f7 cdecl */
void guest_fn_111527(uint32_t a, uint32_t b); /* 0x111527 cdecl */
void guest_fn_10677c(uint32_t a, uint32_t b); /* 0x10677c cdecl */
void guest_fn_10679c(uint32_t a, uint32_t b); /* 0x10679c cdecl */
void guest_fn_1067cc(uint32_t a, uint32_t b); /* 0x1067cc cdecl */
void guest_fn_10682c(uint32_t a, uint32_t b); /* 0x10682c cdecl */
void guest_fn_106988(uint32_t a, uint32_t b); /* 0x106988 cdecl */
void guest_fn_bc400(uint32_t stream, uint32_t format, ...); /* 0xbc400 cdecl varargs */
ef_addr32 guest_fn_eb520(uint32_t key, uint32_t index); /* 0xeb520 EAX/EDX */
void guest_fn_a7990(uint32_t flags); /* 0xa7990 stack */
uint32_t guest_fn_a7ac8(uint32_t amount, uint32_t flags); /* 0xa7ac8 stack */
ef_addr32 guest_fn_ebd30(uint32_t value); /* 0xebd30 */
ef_addr32 guest_fn_ebd36(uint32_t value); /* 0xebd36 */
void guest_fn_a4cb4(uint32_t value); /* 0xa4cb4 EAX */
void guest_fn_a4b80(uint32_t value); /* 0xa4b80 EAX */
uint32_t guest_fn_10f4df(uint32_t handle); /* 0x10f4df EAX */
void guest_fn_f2320(int32_t x, int32_t y, uint32_t ch); /* 0xf2320 EAX/EDX/EBX */
uint32_t guest_fn_b1b8b(ef_addr32 text); /* 0xb1b8b EAX -> AX */
void guest_fn_f2819(void); /* 0xf2819 */
void guest_fn_f2cd0(void); /* 0xf2cd0 */
void guest_fn_9d749(ef_addr32 destination, ef_addr32 source); /* 0x9d749 EAX/EDX */
void guest_fn_dc250(void); /* 0xdc250 */
void guest_fn_c386c(void); /* 0xc386c */
void guest_fn_ddae0(void); /* 0xddae0 */
void guest_fn_dd8dc(void); /* 0xdd8dc */
void guest_fn_c3974(void); /* 0xc3974 */
void guest_fn_eab44(void); /* 0xeab44 */
void guest_fn_c39ac(void); /* 0xc39ac */
void guest_fn_c3990(void); /* 0xc3990 */
uint32_t guest_fn_9d4cd(ef_addr32 text); /* 0x9d4cd EAX */
void guest_fn_9ca57(ef_addr32 first, ef_addr32 second); /* 0x9ca57 EAX/EDX */
void guest_fn_f3a1c(ef_addr32 text); /* 0xf3a1c EAX */
ef_addr32 guest_fn_a7ec4(ef_addr32 text); /* 0xa7ec4 EAX */
uint32_t guest_fn_cc76d(ef_addr32 text); /* 0xcc76d EAX */
void guest_fn_f2b7c(void); /* 0xf2b7c */
void guest_fn_f3b78(void); /* 0xf3b78 */
void guest_fn_f9dcb(uint32_t value, ef_addr32 buffer); /* 0xf9dcb EAX/EDX */
uint32_t guest_fn_1101be(uint32_t handle); /* 0x1101be EAX */
void guest_fn_fba6c(uint32_t first, uint32_t second); /* 0xfba6c EAX/EDX */
void guest_fn_c24(int32_t high, uint32_t address); /* 0xc24 EAX/EDX */

/* runtime_helpers.c */
void trace_call_103244(uint32_t a, uint32_t b);
void trace_call_1033c8(uint32_t a, uint32_t b);
void trace_call_103504(uint32_t a);
void trace_call_1037e0(uint32_t a, uint32_t b);
void trace_call_103b5c(uint32_t a, uint32_t b);
void trace_call_103b90(uint32_t a, uint32_t b, uint32_t c);
void shutdown_103d48(void);
uint32_t exchange_slot_1041e4(uint32_t index, uint32_t value);
void shutdown_104694(void);
uint32_t allocate_and_init_1047a4(uint32_t size);
void free_block_1047c4(uint32_t block, uint32_t size);
void stack_range_104908(uint32_t base, uint32_t length);
void stack_range_104920(uint32_t base, uint32_t length);
void stop_object_105334(ef_addr32 object);
void set_state_1_106634(ef_addr32 object);
uint32_t get_state_106708(ef_addr32 object);
void set_range_106718(ef_addr32 object, uint32_t first, uint32_t second);
void set_pair_106740(ef_addr32 object, uint32_t first, uint32_t second);
void set_field_10677c(ef_addr32 object, uint32_t value);
void set_field_10679c(ef_addr32 object, uint32_t value);
void set_field_1067cc(ef_addr32 object, uint32_t value);
void set_field_10682c(ef_addr32 object, uint32_t value);
void stop_selected_10683c(ef_addr32 object);
void pause_object_10687c(ef_addr32 object);
void resume_object_106894(ef_addr32 object);
uint32_t exchange_callback_106938(ef_addr32 object, uint32_t value);
void set_and_notify_106988(ef_addr32 object, uint32_t value);
int32_t times_100(int32_t value);
int32_t times_100_div_10(int32_t value);
uint8_t elapsed_at_least(int32_t start, int32_t duration);
void release_object_10703c(ef_addr32 slot);
void send_command_107060(ef_addr32 slot);
ef_addr32 build_char_set(ef_addr32 text, ef_addr32 set);
int32_t hex_digit_value(int32_t c);
ef_addr32 strip_cr_inplace(ef_addr32 text);
ef_addr32 copy_line(ef_addr32 destination, ef_addr32 source);
void skip_class_chars(ef_addr32 pointer);
ef_addr32 copy_padded(ef_addr32 destination, ef_addr32 source, uint32_t count);
ef_addr32 find_last_char(ef_addr32 text, uint8_t ch);
void unlink_node_10d427(ef_addr32 node);
int32_t count_matching_streams(uint32_t mask);
uint32_t field_10dfe0(void);
void guest_fn_1141a0(uint32_t a, uint32_t b); /* 0x1141a0 cdecl */
void guest_fn_1142cc(uint32_t a, uint32_t b); /* 0x1142cc cdecl */
void guest_fn_113c24(uint32_t a); /* 0x113c24 cdecl */
void guest_fn_11458c(uint32_t a, uint32_t b); /* 0x11458c cdecl */
void guest_fn_114878(uint32_t a, uint32_t b); /* 0x114878 cdecl */
void guest_fn_114994(uint32_t a, uint32_t b, uint32_t c); /* 0x114994 cdecl */
void guest_fn_1048b4(uint32_t first, uint32_t last); /* 0x1048b4 cdecl */
void guest_fn_104840(uint32_t first, uint32_t last); /* 0x104840 cdecl */
void guest_fn_1023b0(void); /* 0x1023b0 */
void guest_fn_1110c5(void); /* 0x1110c5 */
uint32_t guest_fn_c3f70(uint32_t size); /* 0xc3f70 EAX */
void guest_fn_101f88(uint32_t handle, uint32_t message, uint32_t a, uint32_t b); /* 0x101f88 cdecl */
void guest_fn_10535c(ef_addr32 object); /* 0x10535c cdecl */
void guest_fn_1052f0(uint32_t handle); /* 0x1052f0 cdecl */
uint32_t guest_fn_f8248(void); /* 0xf8248 */
void guest_fn_118290(uint32_t object); /* 0x118290 EAX */
void guest_fn_fa9d2(uint32_t object); /* 0xfa9d2 EAX */
void guest_fn_118428(uint32_t object, uint32_t command); /* 0x118428 EAX/EDX */
void guest_fn_fa8a0(ef_addr32 stream); /* 0xfa8a0 EAX */

/* runtime_libc.c */
void call_11d258_default(void);
ef_addr32 unsigned_to_text(uint32_t value, ef_addr32 destination, uint32_t radix);
ef_addr32 signed_to_text(int32_t value, ef_addr32 destination, uint32_t radix);
uint32_t is_leap_year(int32_t year);
uint32_t compare_dates_10e957(uint32_t first, uint32_t second, uint32_t third);
uint32_t triple_less_than(ef_addr32 first, ef_addr32 second);
ef_addr32 parse_decimal(ef_addr32 text, ef_addr32 result);
int32_t stream_getc(ef_addr32 stream);
uint32_t call_fa95f_from_start(uint32_t handle);
uint32_t elapsed_since_start(void);
uint32_t call_1100d2_default(uint32_t value);
void store_flagged_value(uint32_t index, uint32_t value);
uint32_t lookup_slot_11024e(int32_t index);
void increment_17d7ba(void);
void decrement_17d7ba(void);
uint32_t scale_110f3c(uint32_t value);
uint32_t value_17d7b6(void);
uint32_t value_17d7ae(void);
uint32_t exchange_slot_1113ac(uint32_t index, uint32_t value);
void for_each_slot_1113fb(void);
void for_each_slot_11144f(void);
void for_each_slot_1114a3(void);
void set_rate_1114f7(uint32_t slot, uint32_t divisor);
void set_period_111527(uint32_t slot, uint32_t value);
void guest_fn_11d258(uint32_t a, uint32_t b); /* 0x11d258 EAX/EDX */
uint32_t guest_fn_10e872(uint32_t a, uint32_t b); /* 0x10e872 EAX/EDX */
uint32_t guest_fn_10f1bf(ef_addr32 stream); /* 0x10f1bf EAX */
uint32_t guest_fn_fa95f(uint32_t handle, uint32_t a, uint32_t b); /* 0xfa95f EAX/EDX/EBX */
uint32_t guest_fn_11004b(void); /* 0x11004b */
uint32_t guest_fn_1100d2(uint32_t value, ef_addr32 buffer); /* 0x1100d2 EAX/EDX */
uint32_t guest_fn_110f0c(uint32_t value); /* 0x110f0c cdecl */
uint32_t guest_fn_101e70(void); /* 0x101e70 */
void guest_fn_101e78(void); /* 0x101e78 */

/* game_state_helpers.c */
void init_far_stub_b04(void);
ef_addr32 record_for_kind(int32_t kind);
void tick_selector_1ef8(void);
void reset_selection_3cf0(void);
void select_entry_4bf8(void);
void init_sequence_5f60(void);
uint8_t object_category(ef_addr32 object);
void reset_slots_6e18(void);
void update_unit_90c0(int16_t index);
uint8_t read_buffered_char(void);
uint32_t read_record_ee0c(ef_addr32 record);
uint32_t read_record_eefc(ef_addr32 record);
uint32_t read_record_efe8(ef_addr32 record);
void read_record_f13c(ef_addr32 record);
uint8_t has_free_entry_117d4(void);
void sync_flag_12ec8(void);
uint32_t unit_in_state_169b4(int32_t index);
uint32_t record_kind_matches_16ab4(uint32_t index);
uint32_t cell_blocked_17480(uint32_t index, int32_t x);
void adjust_level_1cd90(void);
void select_tables_1d3c4(uint16_t code);
uint32_t guest_fn_a60(void); /* 0xa60 */
uint32_t guest_fn_510(uint32_t value); /* 0x510 EAX */
uint32_t guest_fn_604(void); /* 0x604 */
void guest_fn_a9c(uint32_t first, uint32_t second); /* 0xa9c EAX/EDX */
void guest_fn_9c3cf(uint32_t value); /* 0x9c3cf EAX */
uint32_t guest_fn_f60(uint32_t value); /* 0xf60 EAX */
void guest_fn_2fbc4(ef_addr32 buffer); /* 0x2fbc4 EAX */
void guest_fn_e710(void); /* 0xe710 */
void guest_fn_45a4(void); /* 0x45a4 */
void guest_fn_aca88(void); /* 0xaca88 */
void guest_fn_5f944(void); /* 0x5f944 */
void guest_fn_121f0(void); /* 0x121f0 */
void guest_fn_12160(void); /* 0x12160 */
void guest_fn_64fa4(void); /* 0x64fa4 */
void guest_fn_65960(void); /* 0x65960 */
void guest_fn_5f634(void); /* 0x5f634 */
void guest_fn_1f960(void); /* 0x1f960 */
void guest_fn_1f0c8(void); /* 0x1f0c8 */
void guest_fn_5f8a8(void); /* 0x5f8a8 */
void guest_fn_5f8d4(uint32_t value); /* 0x5f8d4 EAX */
void guest_fn_6bf4(void); /* 0x6bf4 */
void guest_fn_6b1c(void); /* 0x6b1c */
void guest_fn_6b50(void); /* 0x6b50 */
void guest_fn_14ef4(void); /* 0x14ef4 */
void guest_fn_150d4(void); /* 0x150d4 */
void guest_fn_6df0(uint32_t value); /* 0x6df0 EAX */
uint32_t guest_fn_2dba4(int32_t index); /* 0x2dba4 EAX -> AL */
void guest_fn_8f9c(int32_t index); /* 0x8f9c EAX */
void guest_fn_8fe4(int32_t index, uint32_t value); /* 0x8fe4 EAX/EDX */
uint32_t guest_fn_9cde8(ef_addr32 buffer, uint32_t size, uint32_t handle); /* 0x9cde8 EAX/EDX/EBX */
void guest_fn_e810(void); /* 0xe810 */
void guest_fn_e89c(void); /* 0xe89c */
uint32_t guest_fn_e95c(void); /* 0xe95c */
uint32_t guest_fn_ed30(void); /* 0xed30 */
uint32_t guest_fn_eaf8(void); /* 0xeaf8 */
uint32_t guest_fn_ec8c(void); /* 0xec8c */
uint32_t guest_fn_e990(void); /* 0xe990 */
uint32_t guest_fn_e980(void); /* 0xe980 */
uint32_t guest_fn_ef98(void); /* 0xef98 */
uint32_t guest_fn_f0ec(void); /* 0xf0ec */
uint32_t guest_fn_16984(int32_t index); /* 0x16984 EAX -> AX */
uint32_t guest_fn_174f8(uint32_t index); /* 0x174f8 EAX -> AL */

/* game_state_helpers2.c */
void update_record_1d700(int16_t index);
void push_event_1f878(uint32_t entry);
uint8_t confirm_dialog_27858(void);
void init_chain_28830(void);
void refresh_object_2a168(ef_addr32 object);
void init_entries_2a720(void);
void init_entries_2a7ac(ef_addr32 text, uint32_t slot);
void advance_position_316b8(void);
void advance_position_31aec(void);
void update_angles_31d10(void);
void save_position_32260(void);
void show_notice_34390(void);
uint8_t check_files_343dc(void);
void compact_table_3458c(void);
void scan_entries_34f30(void);
void reset_flags_35144(void);
void select_screen_35ab8(void);
void copy_counts_35bb8(void);
void sync_list_363c4(void);
uint8_t any_accepts_37d3c(int16_t index);
uint8_t all_accept_37e60(int16_t index);
void redraw_screen_39204(void);
void apply_pending_394e0(void);
void reset_table_39dc4(void);
uint32_t guest_fn_121cc(uint32_t a, uint32_t b); /* 0x121cc */
void guest_fn_12d90(uint32_t a, uint32_t b); /* 0x12d90 */
void guest_fn_1d5cc(ef_addr32 p, int32_t v); /* 0x1d5cc */
void guest_fn_1d778(ef_addr32 record); /* 0x1d778 */
void guest_fn_1d7a4(ef_addr32 record); /* 0x1d7a4 */
uint32_t guest_fn_1d8a8(ef_addr32 record); /* 0x1d8a8 */
void guest_fn_1d954(ef_addr32 record); /* 0x1d954 */
uint32_t guest_fn_2949c(int32_t a, uint32_t b); /* 0x2949c */
void guest_fn_2a1fc(int32_t a, uint32_t b); /* 0x2a1fc */
void guest_fn_32414(uint32_t a, uint32_t b, uint32_t c); /* 0x32414 */
void guest_fn_34d94(uint32_t value); /* 0x34d94 */
void guest_fn_35218(void); /* 0x35218 */
uint32_t guest_fn_37c64(int32_t index, uint32_t sub); /* 0x37c64 */
uint32_t guest_fn_37cdc(int32_t index); /* 0x37cdc */
uint32_t guest_fn_37d88(int32_t index, uint32_t sub); /* 0x37d88 */
uint32_t guest_fn_37e00(int32_t index); /* 0x37e00 */
void guest_fn_391d0(void); /* 0x391d0 */
uint32_t guest_fn_39744(int32_t a, uint32_t b, int32_t c, uint32_t d); /* 0x39744 */
void guest_fn_39db0(uint32_t v); /* 0x39db0 */
void guest_fn_6b54c(ef_addr32 object); /* 0x6b54c */
uint32_t guest_fn_9ddfc(ef_addr32 text, ef_addr32 entry); /* 0x9ddfc */
uint32_t guest_fn_a4380(ef_addr32 a, ef_addr32 b); /* 0xa4380 */
ef_addr32 guest_fn_a46cc(ef_addr32 text); /* 0xa46cc */
void guest_fn_a4d90(ef_addr32 text); /* 0xa4d90 */
void guest_fn_a72d8(ef_addr32 node); /* 0xa72d8 */
void guest_fn_a7380(ef_addr32 node); /* 0xa7380 */
void guest_fn_ac7a8(ef_addr32 text); /* 0xac7a8 */
void guest_fn_ac9a0(ef_addr32 dialog); /* 0xac9a0 */
void guest_fn_c34cc(ef_addr32 text); /* 0xc34cc */
void guest_fn_d1e50(uint32_t v); /* 0xd1e50 */
void guest_fn_d65e8(void); /* 0xd65e8 */
void guest_fn_dd370(uint32_t a, uint32_t b, uint32_t c, ef_addr32 d); /* 0xdd370 */
void guest_fn_de580(ef_addr32 p); /* 0xde580 */
uint32_t guest_fn_de864(ef_addr32 text); /* 0xde864 */
void guest_fn_e1730(void); /* 0xe1730 */
void guest_fn_e1e6c(ef_addr32 text, uint32_t flag); /* 0xe1e6c */

/* game_state_helpers3.c */
void run_timed_events_3cf08(void);
void run_timed_events_3db9c(void);
uint32_t code_in_set_41e24(uint32_t code);
void set_unit_mode_488f8(int16_t unit, uint8_t mode);
void update_marker_4bc80(ef_addr32 object);
void clear_pending_4ca90(void);
void draw_sprite_4d114(void);
void draw_sprite_4d180(void);
void set_ranges_4d5f0(void);
void step_sequence_54328(void);
void copy_message_54ba0(ef_addr32 message);
void handle_message_54ff4(ef_addr32 message);
void update_view_557e8(void);
uint8_t remember_value_56018(uint32_t value);
void adjust_level_5c07c(void);
void set_indicator_5f8d4(uint8_t mode);
void init_free_list_64fa4(void);
void release_entry_65360(uint8_t type, uint16_t id);
void check_range_65748(ef_addr32 object);
void check_range_657c4(ef_addr32 object);
uint8_t any_special_slot_6a090(void);
void notify_unit_6b444(int16_t unit);
void guest_fn_151c4(uint32_t a); /* 0x151c4 */
void guest_fn_46168(int32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0x46168 */
void guest_fn_4c78c(ef_addr32 a, ef_addr32 b); /* 0x4c78c */
void guest_fn_4c900(ef_addr32 a, ef_addr32 b); /* 0x4c900 */
void guest_fn_4f314(ef_addr32 a, ef_addr32 b); /* 0x4f314 */
void guest_fn_4f7d0(void); /* 0x4f7d0 */
void guest_fn_544c0(void); /* 0x544c0 */
void guest_fn_54600(void); /* 0x54600 */
void guest_fn_546e8(int32_t unit, uint32_t v); /* 0x546e8 */
void guest_fn_54c10(int32_t unit); /* 0x54c10 */
ef_addr32 guest_fn_7489c(int32_t unit); /* 0x7489c */
void guest_fn_759c(int32_t v); /* 0x759c */
ef_addr32 guest_fn_87f9c(uint32_t a, uint32_t b); /* 0x87f9c */
void guest_fn_8f644(uint32_t v); /* 0x8f644 */
void guest_fn_9e0c4(int32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x9e0c4 */
void guest_fn_ca620(int32_t address, int32_t y, int32_t x, uint32_t a, uint32_t b, uint32_t c); /* 0xca620 */

#endif
