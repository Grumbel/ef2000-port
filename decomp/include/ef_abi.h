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

/* unit_flags.c */
void notify_player_unit_6c3d0(void);
void refresh_player_6c8e0(void);
void advance_state_7104c(void);
void adjust_zoom_7baa4(void);
uint8_t object_glyph_7fb9c(ef_addr32 object);
void reset_menu_80120(void);
uint32_t prepare_block_81308(void);
void trigger_sound_8160c(uint16_t index);
void select_object_895bc(ef_addr32 object);
void select_object_8960c(ef_addr32 object);
void force_select_898d4(ef_addr32 object);
uint32_t find_cell_8b9e8(int16_t x, int16_t z);
void toggle_flag_8daa0(int16_t unit);
void toggle_flag_8db20(int16_t unit);
void toggle_flag_8db80(int16_t unit);
void toggle_flag_8dbe0(int16_t unit);
void toggle_flag_8dc40(int16_t unit);
void toggle_flag_8dc94(int16_t unit);
void guest_fn_6a1d0(int32_t a, uint32_t b); /* 0x6a1d0 */
void guest_fn_710a4(void); /* 0x710a4 */
void guest_fn_73500(int32_t a); /* 0x73500 */
void guest_fn_73c38(int32_t a); /* 0x73c38 */
void guest_fn_73d1c(int32_t a); /* 0x73d1c */
void guest_fn_7bc0(int32_t a, uint32_t b); /* 0x7bc0 */
void guest_fn_7fe8c(void); /* 0x7fe8c */
void guest_fn_87970(ef_addr32 o); /* 0x87970 */
uint32_t guest_fn_d6f88(void); /* 0xd6f88 */
void guest_fn_d7158(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xd7158 */
uint32_t guest_fn_d75fc(uint32_t a); /* 0xd75fc */

/* camera_helpers.c */
void toggle_flag_8dcf4(int16_t unit);
void update_ticker_90828(void);
void init_entries_90a70(void);
void update_viewpoint_92f98(void);
void step_list_92ffc(void);
void update_camera_94484(void);
void look_at_unit_947c4(int16_t unit);
void look_from_player_94830(int16_t unit);
void look_at_target_94a1c(int16_t unit);
void update_progress_985f4(uint8_t channel, int32_t value);
void clear_tables_9a4d8(void);
void guest_fn_38284(int32_t a); /* 0x38284 */
void guest_fn_84678(void); /* 0x84678 */
uint32_t guest_fn_9125c(int32_t a); /* 0x9125c */
void guest_fn_91c48(uint32_t a); /* 0x91c48 */
void guest_fn_92070(void); /* 0x92070 */
void guest_fn_9457c(ef_addr32 a, ef_addr32 b); /* 0x9457c */
void guest_fn_95018(void); /* 0x95018 */
void guest_fn_98658(uint32_t a); /* 0x98658 */
void guest_fn_9a348(void); /* 0x9a348 */
uint32_t guest_fn_9c960(void); /* 0x9c960 */
void guest_fn_9e3f8(int32_t a); /* 0x9e3f8 */
void guest_fn_d099c(ef_addr32 a, ef_addr32 b); /* 0xd099c */
uint32_t guest_fn_d0b68(ef_addr32 a, uint32_t b); /* 0xd0b68 */
void guest_fn_d0ebc(ef_addr32 a, uint32_t b); /* 0xd0ebc */
uint32_t guest_fn_d13dc(ef_addr32 a, uint32_t b); /* 0xd13dc */

/* line_of_sight.c */
void scale_words_1c60c(void);
uint8_t can_support_238c0(int32_t unit);
void latch_vectors_327e0(void);
void setup_dialog_41028(void);
void submit_view_5a8d0(void);
uint8_t request_los_66bf4(ef_addr32 object);
uint8_t request_los_66cd4(int16_t unit, ef_addr32 object);
uint8_t request_los_66d80(ef_addr32 object);
void guest_fn_5aa64(uint32_t a); /* 0x5aa64 */
void guest_fn_5af50(uint32_t a); /* 0x5af50 */
uint32_t guest_fn_7574c(int32_t a); /* 0x7574c */
uint32_t guest_fn_7b7d8(void); /* 0x7b7d8 */
ef_addr32 guest_fn_8ffe8(int32_t a); /* 0x8ffe8 */
void guest_fn_a4db0(ef_addr32 a); /* 0xa4db0 */
void guest_fn_a73cc(ef_addr32 a, ef_addr32 b, uint32_t c); /* 0xa73cc */
void guest_fn_abe44(ef_addr32 a); /* 0xabe44 */
void guest_fn_cf78c(void); /* 0xcf78c */
uint32_t guest_fn_d6694(void); /* 0xd6694 */

/* entity_helpers.c */
uint32_t find_in_archives_9cb20(uint32_t key);
uint32_t lookup_in_archives_9cb70(uint32_t key);
void close_request_9ce84(uint32_t flags);
int32_t parse_int(ef_addr32 text);
int32_t guest_strcmp(ef_addr32 first, ef_addr32 second);
void clamp_extents_a5000(ef_addr32 object);
void init_entities_a59d8(void);
void update_entities_a5a68(void);
void init_timers_a6940(ef_addr32 object);
void toggle_active_a7328(ef_addr32 object);
void wait_for_key_a89f0(void);
void script_if_equal_a8f80(ef_addr32 op);
void script_if_not_equal_a8fcc(ef_addr32 op);
void script_if_not_greater_a9014(ef_addr32 op);
void latch_previous_acb68(ef_addr32 record);
void guest_fn_a499c(uint32_t i); /* 0xa499c */
void guest_fn_a4ec0(ef_addr32 object); /* 0xa4ec0 */
void guest_fn_a4eec(ef_addr32 object); /* 0xa4eec */
void guest_fn_a5070(ef_addr32 object); /* 0xa5070 */
void guest_fn_a51dc(ef_addr32 object); /* 0xa51dc */
void guest_fn_a54f0(ef_addr32 object); /* 0xa54f0 */
void guest_fn_a59ac(ef_addr32 object); /* 0xa59ac */
void guest_fn_a66a0(ef_addr32 object); /* 0xa66a0 */
void guest_fn_a6998(ef_addr32 object); /* 0xa6998 */
void guest_fn_a6a64(ef_addr32 object); /* 0xa6a64 */
void guest_fn_a729c(ef_addr32 first, ef_addr32 second); /* 0xa729c */
uint32_t guest_fn_a7b3c(uint32_t a); /* 0xa7b3c */
uint32_t guest_fn_b3b90(uint32_t a); /* 0xb3b90 */
void guest_fn_dc2e0(void); /* 0xdc2e0 */
uint32_t guest_fn_eb9f8(uint32_t a, uint32_t b); /* 0xeb9f8 */

/* entity_helpers2.c */
void randomize_orientation_b038c(void);
void perturb_orientation_b0448(void);
void free_object_b1740(ef_addr32 object);
void reset_view_tables_b1b10(void);
int32_t approx_length_b3988(uint32_t first, uint32_t second);
int32_t angle_of_b3b1c(int32_t x, int32_t y);
uint32_t code_in_set_bcb50(uint32_t code);
uint32_t code_pair_matches_bcec0(uint8_t value, uint8_t code);
uint32_t slot_type_is_special_bd490(uint8_t type, uint8_t flag);
uint32_t slot_usable_bd5a8(ef_addr32 object, int16_t slot);
void reset_slots_bd688(ef_addr32 object);
void count_slots_bd6d8(ef_addr32 object);
uint32_t records_differ_bf980(ef_addr32 object);
uint32_t records_differ_bf9d4(ef_addr32 object);
void guest_fn_b1814(ef_addr32 a, ef_addr32 b); /* 0xb1814 */
void guest_fn_b188c(ef_addr32 a, ef_addr32 b); /* 0xb188c */
void guest_fn_b18b8(ef_addr32 a, ef_addr32 b); /* 0xb18b8 */
void guest_fn_b1974(ef_addr32 a, ef_addr32 b); /* 0xb1974 */
void guest_fn_b19e4(ef_addr32 a, ef_addr32 b); /* 0xb19e4 */
void guest_fn_b1a3c(ef_addr32 a); /* 0xb1a3c */
void guest_fn_b1ae4(ef_addr32 a, ef_addr32 b); /* 0xb1ae4 */
uint32_t guest_fn_b3ae0(int32_t x, int32_t y); /* 0xb3ae0 */
uint32_t guest_fn_bd508(ef_addr32 o); /* 0xbd508 */
uint32_t guest_fn_bd554(ef_addr32 o); /* 0xbd554 */
void guest_fn_cbf00(void); /* 0xcbf00 */
uint32_t guest_fn_cf94a(uint32_t v); /* 0xcf94a */
void guest_fn_f8f8(void); /* 0xf8f8 */

/* engine_helpers.c */
void reset_view_c4105(void);
void redraw_ca17c(void);
void rebuild_tables_ca7e0(void);
uint32_t code_in_set_cc584(uint8_t code);
int32_t parse_int_cc76d(ef_addr32 text);
uint32_t pick_variant_cc894(uint8_t code);
uint8_t open_entry_cd150(int16_t number);
void init_keys_cd430(void);
void install_handlers_d1f10(void);
void count_hit_d37f4(ef_addr32 object);
void copy_block_d7c40(void);
uint32_t permute_block_d7e94(void);
void advance_position_d8358(int16_t delta);
void reset_scale_dc3d5(void);
uint32_t query_mouse_ddc88(void);
void mouse_request_de12c(void);
uint32_t mouse_request_de2d8(void);
void mouse_request_de4b0(void);
void guest_fn_8b8(uint32_t code, ef_addr32 block); /* 0x8b8 */
uint32_t guest_fn_a0de0(int32_t n, uint32_t v); /* 0xa0de0 */
uint32_t guest_fn_a1a10(int32_t n); /* 0xa1a10 */
uint32_t guest_fn_a45b3(void); /* 0xa45b3 */
void guest_fn_abde4(void); /* 0xabde4 */
uint32_t guest_fn_c3f50(int32_t x, int32_t y); /* 0xc3f50 */
void guest_fn_c4a50(ef_addr32 a); /* 0xc4a50 */
void guest_fn_c9fc4(void); /* 0xc9fc4 */
void guest_fn_ca874(ef_addr32 a, ef_addr32 b); /* 0xca874 */
void guest_fn_cb9f4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f); /* 0xcb9f4 */
void guest_fn_d663c(ef_addr32 a); /* 0xd663c */
void guest_fn_d7bf0(void); /* 0xd7bf0 */
uint32_t guest_fn_d8170(ef_addr32 a); /* 0xd8170 */
void guest_fn_dc2c0(void); /* 0xdc2c0 */

/* runtime_helpers2.c */
void fill_channel_e0870(void);
uint8_t setup_sound_e1194(void);
uint8_t any_device_e1260(void);
void rewrite_template_ea4c0(void);
uint32_t resolve_entry_eb9f8(uint32_t key, ef_addr32 object);
void parse_format_flags(ef_addr32 text, ef_addr32 spec);
void read_script_line_f2c78(void);
void read_script_token_f2cd0(void);
uint8_t read_buffered_char_f45d8(void);
ef_addr32 push_node_f57d4(void);
void count_slots_f88e8(ef_addr32 object);
void reset_slots_f8938(ef_addr32 object);
uint32_t decrement_f8a78(ef_addr32 object);
uint32_t map_mode_fc08c(uint8_t mode, uint16_t flag);
uint8_t query_video_mode_fc478(uint32_t mode);
uint32_t find_linear_mode_fc4e4(void);
uint32_t guest_fn_b3db0(uint32_t a); /* 0xb3db0 */
void guest_fn_e06ac(void); /* 0xe06ac */
uint32_t guest_fn_e0b10(ef_addr32 a); /* 0xe0b10 */
uint32_t guest_fn_e0b88(ef_addr32 a); /* 0xe0b88 */
void guest_fn_e0bcc(void); /* 0xe0bcc */
void guest_fn_e10a0(ef_addr32 a); /* 0xe10a0 */
void guest_fn_e1110(uint32_t a); /* 0xe1110 */
void guest_fn_e115c(uint32_t a); /* 0xe115c */
void guest_fn_eb698(uint32_t a, ef_addr32 object); /* 0xeb698 */

/* runtime_helpers3.c */
ef_addr32 insert_heap_block_10d5c7(ef_addr32 block);
ef_addr32 alloc_stream_buffer_10f071(ef_addr32 stream);
uint32_t midi_data_length_111d98(uint32_t status);
uint32_t byte_swap_111f50(uint32_t value);
void strip_extension_113c24(ef_addr32 path);
void set_owner_slot_114994(ef_addr32 owner, uint32_t slot, uint32_t index);
void clear_buffered_record_117ea8(ef_addr32 record);
uint32_t alloc_serial_state_117f14(ef_addr32 record);
int32_t ring_pop_1184c4(ef_addr32 record);
void serial_reset_1185ec(ef_addr32 record);
void serial_set_flow_118664(ef_addr32 record, uint32_t enable);
void serial_set_dtr_118694(ef_addr32 record, uint32_t enable);
void serial_set_rts_1186cc(ef_addr32 record, uint32_t enable);
void alloc_records_11c020(uint32_t count);
void free_records_11c06c(void);
int32_t trig_lookup_11f308(int32_t x);
int32_t trig_lookup_11f460(int32_t x);
int32_t trig_full_circle_11f4d8(int32_t angle);
uint32_t guest_fn_9c430(uint32_t port); /* 0x9c430 EAX -> AL, inb */
void guest_fn_117b0c(uint32_t port, uint32_t value); /* 0x117b0c EAX/EDX, outb */
void guest_fn_11f2d3(void); /* 0x11f2d3 */
void guest_fn_11f2d5(void); /* 0x11f2d5 */
ef_addr32 guest_fn_e8733(uint32_t count, uint32_t size); /* 0xe8733 EAX/EDX -> EAX */

/* ui_widget_helpers.c */
int32_t push_handler_9c3cf(uint32_t handler);
void copy_string_9d749(ef_addr32 destination, ef_addr32 source);
void set_axis_9d7ac(uint32_t a, uint32_t b);
void set_axis_9d80c(uint32_t a, uint32_t b);
void reset_parser_state_9d938(void);
uint32_t parse_decimal_9d964(ef_addr32 text);
ef_addr32 skip_line_9da50(ef_addr32 text);
ef_addr32 page_slot_a0da8(int16_t index);
ef_addr32 select_page_slot_a0de0(int16_t page_id, int16_t index);
void clear_page_slot_a0f4c(int16_t index);
void flag_page_slot_a14c8(int16_t index);
int32_t record_type_value_a17e8(int16_t id);
void copy_record_header_a1824(ef_addr32 destination, int16_t id);
void refresh_children_a6998(ef_addr32 widget);
void redraw_children_a7104(ef_addr32 widget);
void redraw_all_children_a7260(ef_addr32 widget);
void notify_group_a729c(ef_addr32 widget, ef_addr32 other);
void enable_widget_a72d8(ef_addr32 widget);
void save_table_a7e4c(void);
void restore_table_a7e88(void);
void apply_operation_a983c(ef_addr32 op);
void guest_fn_a66cc(ef_addr32 widget, uint32_t mode); /* 0xa66cc EAX/EDX */
void guest_fn_a6808(ef_addr32 widget); /* 0xa6808 EAX */

/* list_helpers.c */
void append_node_aa1e4(ef_addr32 node);
void append_child_aa228(ef_addr32 node);
void append_node_aa270(ef_addr32 node);
void resolve_child_aa2b4(ef_addr32 node);
void advance_counters_b0404(void);
void free_entry_array_b16c0(ef_addr32 record);
void free_entry_array_b1700(ef_addr32 record);
void free_object_array_b17d4(ef_addr32 record);
void update_ring_b3d74(void);
void fill_level_table_ba40c(ef_addr32 table, uint8_t mode);
void unlink_node_bc594(ef_addr32 node, ef_addr32 head_a, ef_addr32 head_b);
uint32_t is_kind_bcbc0(uint32_t kind);
uint32_t find_kind10_bd344(ef_addr32 ref, ef_addr32 list);
uint32_t is_active_bec84(ef_addr32 object);

/* engine_helpers3.c */
void build_default_path_9c980(ef_addr32 buffer);
void release_mode_buffer_c3328(void);
void free_buffers_c37ec(void);
void clear_record_flags_ca5e4(void);
uint32_t group_flag_negative_cc1ac(uint8_t group, uint32_t index, ef_addr32 head_addr);
uint32_t group_flag_zero_cc1ec(uint8_t group, uint32_t index, ef_addr32 head_addr);
uint32_t group_flag_positive_cc22c(uint8_t group, uint32_t index, ef_addr32 head_addr);
uint32_t script_store_step_cd20c(uint32_t value);
int32_t queue_pop_cd5f4(void);
uint32_t mouse_button_count_cdf60(void);
void mouse_read_motion_ce02c(ef_addr32 out_x, ef_addr32 out_y);
uint32_t mouse_read_buttons_ce064(void);
void latch_values_ce328(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
ef_addr32 apply_pending_events_ce944(ef_addr32 object);
void forward_record_cf78c(void);
void copy_record9_d0858(ef_addr32 destination, ef_addr32 source);
void init_bounds_d0b68(ef_addr32 record);
void remap_palette_d65a8(ef_addr32 destination, ef_addr32 source);
void init_module_d6dc4(void);
void build_path_d7040(ef_addr32 buffer, ef_addr32 second);
void reset_clip_dcc10(void);
void reset_clip_dcc54(void);
void guest_fn_eb478(ef_addr32 text); /* 0xeb478 EAX */
void guest_fn_ce830(ef_addr32 object, uint32_t value); /* 0xce830 EAX/EDX */
void guest_fn_cf3a4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0xcf3a4 EAX/EDX/EBX/ECX/stack */
void guest_fn_101c54(void); /* 0x101c54 */
void guest_fn_d6df0(void); /* 0xd6df0 */
void guest_fn_cdf48(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xcdf48 EAX/EDX/EBX/ECX */

/* runtime_helpers4.c */
uint32_t open_device_e0b88(ef_addr32 value);
void refresh_slot_e5830(ef_addr32 object);
void compute_rect_e5960(ef_addr32 record);
int32_t compare_strings_e5c03(ef_addr32 a, ef_addr32 b, uint32_t count);
void enter_handler_eb448(uint32_t value);
void uint_to_extended_eff7f(uint32_t value, ef_addr32 destination);
void or_flag_mask_f79b0(void);
uint32_t check_entry_f88a0(ef_addr32 object, int16_t index);
void release_list_node_fa77b(ef_addr32 object);
uint8_t classify_kind_fc050(uint8_t kind);
void query_video_fc6c8(uint32_t value);
void guest_fn_e10e0(void); /* 0xe10e0 */
void guest_fn_e59cc(ef_addr32 target, ef_addr32 source); /* 0xe59cc EAX/EDX */
void guest_fn_eb25a(uint32_t value); /* 0xeb25a EAX */
void guest_fn_f6124(void); /* 0xf6124 */
uint32_t guest_fn_bd4e0(uint32_t value); /* 0xbd4e0 EAX -> AX */
uint32_t guest_fn_f8a30(ef_addr32 object); /* 0xf8a30 EAX -> AX */
uint32_t guest_fn_f89e0(ef_addr32 object); /* 0xf89e0 EAX -> AX */

/* tiny_helpers.c */
uint32_t far_to_linear_9c024(uint32_t value);
uint32_t high_word_9c1a1(uint32_t value);
uint32_t identity_9c1b1(uint32_t value);
uint32_t half_word_9c960(void);
void copy_head_9d780(ef_addr32 destination, ef_addr32 source);
void set_axis_9d794(uint32_t a, uint32_t b);
uint16_t store_table_entry_9dde0(ef_addr32 destination, uint32_t index);
void set_flag_a41c8(void);
void clear_flag_a41d0(void);
void call_edd49_a455b(void);
ef_addr32 apply_a4cb4_a4d90(ef_addr32 key);
ef_addr32 apply_a4b80_a4db0(ef_addr32 key);
ef_addr32 apply_a5ef8_a6018(ef_addr32 key);
ef_addr32 enable_found_widget_a7318(ef_addr32 key);
uint32_t first_link_a75fc(ef_addr32 object);
ef_addr32 default_path_a7b70(void);
uint32_t read_counter_b3d68(void);
void clear_pair_baf24(ef_addr32 pair);
uint32_t release_by_key_bc450(ef_addr32 key);
void clear_byte_c8e38(void);
void clear_byte_c8fdc(void);
void clear_byte_c90c0(void);
ef_addr32 signed_to_digits_cbd0f(int32_t value, ef_addr32 buffer, uint32_t base);
ef_addr32 script_cursor_data_cd29c(void);
void call_d16d0_d16fc(uint32_t value);
uint32_t read_byte_d6f88(void);
ef_addr32 build_path_d7070(ef_addr32 second);
uint32_t read_field_d7738(ef_addr32 record);
uint32_t read_field_d7748(ef_addr32 record);
void append_two_e7cd0(ef_addr32 first, ef_addr32 second);
uint32_t read_link_ea414(void);
void store_triple_ea864(ef_addr32 destination, uint32_t a, uint32_t b, uint32_t c);
void refresh_object_f13ac(ef_addr32 object);
void guest_fn_edd49(void); /* 0xedd49 */
ef_addr32 guest_fn_a78bc(ef_addr32 key); /* 0xa78bc EAX -> EAX */
uint32_t guest_fn_bc4da(ef_addr32 key); /* 0xbc4da EAX -> EAX */
void guest_fn_cbcc5(uint32_t value, ef_addr32 buffer, uint32_t base); /* 0xcbcc5 EAX/EDX/EBX */
void guest_fn_d16d0(uint32_t value, uint32_t mode); /* 0xd16d0 EAX/EDX */

/* runtime_helpers5.c */
void emit_display_list_d17d0(void);
void reset_engine_e4264(void);
void merge_flag_fields_f8010(void);
uint32_t set_video_mode_fc600(uint32_t mode);
void guest_fn_d170c(uint32_t value); /* 0xd170c EAX */
void guest_fn_d175c(uint32_t value); /* 0xd175c EAX */
void guest_fn_d1860(uint32_t value); /* 0xd1860 EAX */
void guest_fn_cd7a0(void); /* 0xcd7a0 */

/* ui_state_helpers.c */
void recompute_total_a4030(void);
uint32_t handle_key_a7be8(uint8_t modifier);
void toggle_view_state_a9d48(void);
void swap_records_e65d8(ef_addr32 a, ef_addr32 b);
uint8_t scan_code_class_faa30(uint8_t code);
void guest_fn_a3fe0(void); /* 0xa3fe0 */
void guest_fn_a410c(void); /* 0xa410c */
void guest_fn_a3da0(void); /* 0xa3da0 */
void guest_fn_a7df4(void); /* 0xa7df4 */
void guest_fn_a7e20(void); /* 0xa7e20 */
void guest_fn_f41c4(void); /* 0xf41c4 */

/* game_helpers4.c */
void reinit_screens_5fd8(void);
void update_flag_6b1c(void);
int32_t pick_count_a9a8(void);
void apply_two_a4b80_b544(void);
uint32_t fill_random_ef5c(ef_addr32 destination);
void run_12d90_pair_13950(void);
uint32_t unit_active_16984(uint32_t index);
uint32_t unit_kind1_with_target_16a04(uint32_t index);
uint32_t unit_kind_6_or_7_16a3c(uint32_t index);
uint32_t unit_kind_b_or_flag_16a74(uint32_t index);
uint32_t find_slot_kind_17434(int16_t unit_id);
uint32_t unit_target_differs_174c8(uint32_t index, int32_t value);
uint32_t unit_kind_allows_174f8(uint32_t index);
uint32_t selected_unit_visible_1b52c(void);
uint32_t guest_fn_173c4(uint32_t index); /* 0x173c4 EAX -> AX */

/* game_helpers5.c */
ef_addr32 alloc_cleared_e744(uint32_t count);
void select_stage_byte_1d778(ef_addr32 object);
uint32_t advance_stage_1d8a8(ef_addr32 object);
void forget_record_1e130(int16_t index);
void free_list_node_28a34(void);
void set_flag_ranges_2af20(void);
void reset_flag_ranges_2af64(void);
uint32_t unit_has_id_2db64(int16_t id, uint32_t slot);
void reset_state_35528(void);
void start_screen_357a4(void);
void mark_slot_35cc8(void);
ef_addr32 find_tagged_node_3cd6c(void);
void tick_timer_3ce30(void);
uint32_t is_kind_3_to_10_41ec8(uint32_t kind);
void clear_slots_41f24(void);
void reset_queue_46038(void);
void advance_ticks_48020(void);
void check_flags_4be88(ef_addr32 object);
void reset_text_50c70(void);
uint32_t text_ends_with_colon_50dc8(void);
void note_extension_52128(void);
void mark_slot_54eb4(ef_addr32 message);
void mark_slot_54f04(ef_addr32 message);
void clear_pending_55050(ef_addr32 message);
void copy_payload_550d8(ef_addr32 message);
void set_name_56d18(uint32_t value);
void handle_message_56e94(ef_addr32 message);
void notify_unit_57160(ef_addr32 message);
void notify_unit_571b8(ef_addr32 message);
void handle_entry_57620(ef_addr32 message);
void reset_range_5ea64(void);
void process_entries_62504(void);
void store_631bc_65d18(void);
void copy_table_689f0(void);
void request_68a2c(void);
void clear_unit_flag_73d1c(int16_t unit_id);
void reset_selection_7597c(void);
void reset_slots_77be0(void);
uint32_t guest_fn_1d8e8(ef_addr32 object); /* 0x1d8e8 EAX -> AL */
void guest_fn_a42f4(void); /* 0xa42f4 */
void guest_fn_b1e0(void); /* 0xb1e0 */
void guest_fn_413b0(void); /* 0x413b0 */
void guest_fn_45ba0(void); /* 0x45ba0 */
void guest_fn_4ce4c(uint32_t a, uint32_t b); /* 0x4ce4c EAX/EDX */
void guest_fn_50d04(void); /* 0x50d04 */
void guest_fn_56d40(uint32_t value); /* 0x56d40 EAX */
void guest_fn_789d0(uint32_t a, ef_addr32 b); /* 0x789d0 EAX/EDX */
void guest_fn_56ed0(void); /* 0x56ed0 */
void guest_fn_cd2e8(int32_t value); /* 0xcd2e8 EAX */
void guest_fn_65030(uint32_t a, int32_t b, uint32_t c); /* 0x65030 EAX/EDX/EBX */
void guest_fn_e7c50(int32_t a, int32_t b, uint32_t c); /* 0xe7c50 EAX/EDX/EBX */
uint32_t guest_fn_631bc(void); /* 0x631bc -> AX */

/* game_helpers6.c */
void refresh_all_624f4(void);
uint32_t unit_slot_value_7556c(int16_t unit_id);
void expand_table_7632c(void);
void pool_append_7a51c(ef_addr32 text);
ef_addr32 pool_take_7a54c(void);
void flush_pool_7a5bc(void);
void alloc_buffers_7b3c8(void);
void reset_menu_7eec4(void);
void show_menu_item_802e0(int16_t item);
void show_menu_item_80314(void);
ef_addr32 find_entry_815b4(ef_addr32 key);
uint8_t advance_state_8461c(void);
uint8_t unit_has_slot_84a80(int16_t unit_id);
void reset_flags_85b70(void);
void process_ring_880c8(void);
void activate_8965c(ef_addr32 object);
void activate_89990(ef_addr32 object);
uint8_t table_contains_8c064(uint32_t value);
void maybe_retarget_8d554(int16_t unit_id);
void update_view_94880(ef_addr32 argument);
void reset_mode_98280(void);
void select_handlers_982a0(void);
ef_addr32 alloc_zeroed_984f0(uint32_t size);
void shift_tables_9bb64(ef_addr32 record);
void guest_fn_6454c(void); /* 0x6454c */
void guest_fn_62edc(void); /* 0x62edc */
void guest_fn_62b84(void); /* 0x62b84 */
void guest_fn_7a480(void); /* 0x7a480 */
void guest_fn_7a3b0(ef_addr32 text, uint32_t start, uint32_t length); /* 0x7a3b0 EAX/EDX/EBX */
void guest_fn_b3d00(void); /* 0xb3d00 */
void guest_fn_d76f4(uint32_t value, uint32_t flags); /* 0xd76f4 EAX/EDX */
void guest_fn_802ac(uint32_t value); /* 0x802ac EAX */
uint32_t guest_fn_d771c(uint32_t value); /* 0xd771c EAX -> EAX */
void guest_fn_7fbec(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x7fbec EAX/EDX/EBX/ECX */
void guest_fn_878c8(ef_addr32 node); /* 0x878c8 EAX */
void guest_fn_725dc(int32_t unit); /* 0x725dc EAX */
void guest_fn_980e8(uint32_t value); /* 0x980e8 EAX */
uint32_t guest_fn_ddbe0(void); /* 0xddbe0 -> AL */

/* game_helpers7.c */
uint32_t apply_slot_7dec(int16_t unit_id, uint8_t table, uint8_t index);
uint8_t find_free_slot_825c(uint8_t table);
void update_table_slots_8f4c(uint8_t table);
void run_unit_slots_8f9c(int16_t unit_id);
uint32_t random_clamped_ab2c(int16_t center, int16_t spread);
uint32_t find_entry_11770(uint32_t a, uint32_t b, uint32_t c);
uint32_t take_charge_17538(int16_t index);
uint32_t point_in_box_1e1c0(ef_addr32 box, int32_t x, int32_t y, int32_t z);
void age_timers_1e684(void);
uint32_t queue_pop_1f8c4(void);
void translate_and_call_237c8(ef_addr32 vector);
void clear_part_flags_266f8(ef_addr32 object);
uint32_t max_magnitude_2888c(uint32_t a, uint32_t b, uint32_t c);
uint8_t try_assign_2b508(int16_t unit_id);
uint32_t find_linked_unit_2bb64(int16_t unit_id);
void update_buttons_33e78(void);
void set_buttons_35fbc(void);
void set_buttons_36004(void);
void set_buttons_36964(void);
void set_buttons_369ac(void);
int32_t count_live_38674(int16_t index);
uint32_t node_position_386f8(int16_t index);
uint32_t guest_fn_7d20(ef_addr32 a, ef_addr32 b); /* 0x7d20 EAX/EDX -> AL */
void guest_fn_8cc4(uint32_t a, uint32_t b); /* 0x8cc4 EAX/EDX */
void guest_fn_46254(uint32_t a, int32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f); /* 0x46254 EAX/EDX/EBX/ECX/stack */
void guest_fn_b39ec(uint32_t a, uint32_t b, uint32_t c); /* 0xb39ec EAX/EDX/EBX */
void guest_fn_26744(ef_addr32 object); /* 0x26744 EAX */
void guest_fn_26960(ef_addr32 object); /* 0x26960 EAX */
void guest_fn_26b60(int32_t value); /* 0x26b60 EAX */
uint32_t guest_fn_2c454(ef_addr32 a, int32_t b, uint32_t c, uint32_t d); /* 0x2c454 EAX/EDX/EBX/ECX -> AX */
void guest_fn_2b558(int32_t unit); /* 0x2b558 EAX */
void guest_fn_33630(void); /* 0x33630 */
void guest_fn_a520c(ef_addr32 button, int32_t value, uint32_t mode, uint32_t flag); /* 0xa520c EAX/EDX/EBX/ECX */

/* game_helpers8.c */
void read_line_3c558(void);
void store_plane_pixel_3c94c(int16_t row, int16_t column, int16_t value);
void draw_grid_3d904(void);
uint8_t all_entries_ready_3da24(void);
void reset_words_45694(void);
void reset_tables_45ba0(void);
void retire_entries_46128(ef_addr32 object);
int32_t max_difference_50770(void);
void release_slot_55784(ef_addr32 message);
void apply_action_55b80(ef_addr32 message);
void bump_counter_562d8(uint32_t key, int16_t slot);
void guest_fn_ceccc(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e); /* 0xceccc EAX/EDX/EBX/ECX/stack */
uint32_t guest_fn_a0e98(int32_t id); /* 0xa0e98 EAX -> AL */
ef_addr32 guest_fn_bfcfc(ef_addr32 a, ef_addr32 b); /* 0xbfcfc EAX/EDX -> EAX */
void guest_fn_2b708(int32_t unit); /* 0x2b708 EAX */
void guest_fn_8873c(int32_t a, ef_addr32 unit, uint32_t c, ef_addr32 target); /* 0x8873c EAX/EDX/EBX/ECX */

/* game_helpers9.c */
void handle_owner_message_57218(ef_addr32 message);
void clear_owner_slot_5736c(ef_addr32 message);
void rebuild_slot_mask_58028(void);
ef_addr32 find_node_653d8(uint32_t kind, uint32_t id);
void fill_random_table_685ac(void);
void reset_view_6900c(void);
uint32_t find_named_record_699f4(ef_addr32 text);
uint32_t next_unit_6a148(uint32_t start);
void load_waypoint_752d0(int16_t unit_id);
uint32_t sum_slot_weights_755c8(int16_t unit_id, uint8_t filter);
void assign_slot_77088(ef_addr32 unit, uint8_t slot);
uint32_t map_value_7ae0c(int32_t a, int32_t b);
uint32_t squared_distance_7f7dc(ef_addr32 vector);
void show_menu_value_8018c(int16_t item, uint32_t value);
void guest_fn_56004(void); /* 0x56004 */
void guest_fn_56208(void); /* 0x56208 */
uint32_t guest_fn_9d524(ef_addr32 a, ef_addr32 b); /* 0x9d524 EAX/EDX */
void guest_fn_76f58(ef_addr32 unit, ef_addr32 record); /* 0x76f58 EAX/EDX */
uint32_t guest_fn_d70a0(uint32_t value); /* 0xd70a0 AL -> AL */

/* game_helpers10.c */
void load_resource_1f0c8(void);
void set_owner_pair_2d894(int16_t owner, uint32_t stack_first, uint32_t stack_second);
void show_status_34a04(void);
void show_status_34a38(void);
void show_status_34a68(void);
void init_list_391d0(void);
void send_unit_command_7ae4(int16_t unit_id, uint8_t a, uint8_t b, uint8_t kind);
uint32_t guest_fn_c3670(ef_addr32 a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0xc3670 EAX/EDX/EBX/ECX/stack, ret 4 */
void guest_fn_2da54(int32_t owner); /* 0x2da54 EAX */
void guest_fn_c36b4(ef_addr32 a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f); /* 0xc36b4 EAX/EDX/EBX/ECX/stack, ret 8 */
void guest_fn_740e4(int32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0x740e4 EAX/EDX/EBX/ECX/stack, ret 4 */

/* game_helpers11.c */
void plot_trail_3cc94(ef_addr32 start);
void init_screen_413b0(void);
void refresh_widget_507dc(void);
uint32_t wait_command_50e84(void);
void reset_selection_540d0(void);
void request_name_54e80(uint32_t value);
void send_slot_command_77034(int16_t unit_id, int16_t value, uint8_t kind);
void guest_fn_3cc28(ef_addr32 text); /* 0x3cc28 EAX */
ef_addr32 guest_fn_3c994(ef_addr32 text); /* 0x3c994 EAX -> EAX */
void guest_fn_50ca4(void); /* 0x50ca4 */
void guest_fn_50d9c(void); /* 0x50d9c */

/* game_helpers12.c */
void send_unit_pending_54688(int16_t unit_id, uint8_t value);
void send_owner_pending_546e8(int16_t owner, uint8_t value);
void send_object_block_54720(int16_t object_id);
void send_object_part_54784(int16_t object_id, int16_t part);
void send_vector_55dac(ef_addr32 vector, uint16_t value);
void send_record_word_55dec(ef_addr32 record);
void send_object_params_55e1c(int16_t object_id, uint8_t kind, uint16_t extra);
void send_unit_params_55e94(int16_t unit_id, uint8_t kind);
void send_object_words_55f0c(int16_t object_id, uint16_t b, uint16_t c, uint16_t d);
void send_triple_56f6c(void);
void send_player_57_570c8(void);
void guest_fn_78aa8(ef_addr32 text); /* 0x78aa8 EAX */

/* runtime_helpers6.c */
uint32_t call_9de08_9ddfc(ef_addr32 text);
uint32_t abs_a28f2(int32_t value);
void set_global_ab838(uint32_t value);
uint32_t get_global_b3d60(void);
uint32_t get_global_b3f48(void);
uint32_t get_global_b3f50(void);
uint32_t get_global_c3824(void);
uint32_t get_global_c382c(void);
int32_t random_modulo_b55a4(ef_addr32 record);
uint32_t guest_fn_9de08(ef_addr32 text, uint32_t flag); /* 0x9de08 EAX/EDX */

/* runtime_helpers7.c */
void build_path_9c9c0(ef_addr32 buffer, ef_addr32 name);
void set_clip_9d7d4(int32_t x, int32_t y);
void append_base_name_9dc88(ef_addr32 scratch, ef_addr32 path, ef_addr32 name);
void clear_page_slot_9f7d0(int16_t index, uint32_t value);
void set_page_slot_9f808(int16_t index, uint32_t value, uint8_t flags, uint32_t position);
void copy_page_words_a0d58(int16_t offset, uint16_t count, ef_addr32 source);
int32_t find_chain_entry_a0018(uint16_t key_high, uint16_t key_low);
uint32_t find_page_pair_a1a90(int16_t page, int32_t key);
void set_page_entry_a2634(int16_t index, int16_t entry, uint32_t value);
uint32_t guest_fn_9c858(ef_addr32 text); /* 0x9c858 EAX -> EAX */
void guest_fn_a207c(ef_addr32 target, ef_addr32 slot); /* 0xa207c EAX/EDX */

/* runtime_helpers8.c */
void install_handlers_a3acc(void);
void dispatch_entry_a33a4(uint32_t entry);
void dispatch_all_a33f0(void);
ef_addr32 find_registered_a4678(ef_addr32 key);
ef_addr32 run_widget_a4dd0(ef_addr32 widget);
void move_widget_a5484(ef_addr32 widget, int16_t x, int16_t y);
void toggle_children_a570c(ef_addr32 widget, uint8_t state);
void guest_fn_a32e8(uint32_t entry); /* 0xa32e8 EAX */
void guest_fn_a334c(uint32_t entry); /* 0xa334c EAX */
void guest_fn_a3824(uint32_t entry); /* 0xa3824 EAX */
void guest_fn_ac5f8(uint32_t value); /* 0xac5f8 EAX */
void guest_fn_ac9e8(void); /* 0xac9e8 */
void guest_fn_a51dc(ef_addr32 widget); /* 0xa51dc EAX */
void guest_fn_a739c(ef_addr32 widget, uint32_t state); /* 0xa739c EAX/EDX */
void guest_fn_a56f8(ef_addr32 widget); /* 0xa56f8 EAX */
void guest_fn_a6038(void); /* 0xa6038 */

/* runtime_helpers9.c */
void store_params_10710(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t stack_e, uint8_t stack_f);
void set_widget_params_a5b1c(ef_addr32 widget, uint32_t a, uint16_t b, uint16_t c, uint16_t stack_d);
uint32_t query_device_d9ba0(uint32_t code, uint32_t arg, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 stack_out_c, ef_addr32 stack_out_d);
void guest_fn_a32e8(uint32_t entry); /* 0xa32e8 EAX */
void guest_fn_a334c(uint32_t entry); /* 0xa334c EAX */
void guest_fn_a3824(uint32_t entry); /* 0xa3824 EAX */
void guest_fn_ac5f8(uint32_t value); /* 0xac5f8 EAX */
void guest_fn_ac9e8(void); /* 0xac9e8 */
void guest_fn_a51dc(ef_addr32 widget); /* 0xa51dc EAX */
void guest_fn_a739c(ef_addr32 widget, uint32_t state); /* 0xa739c EAX/EDX */
void guest_fn_a56f8(ef_addr32 widget); /* 0xa56f8 EAX */
void guest_fn_a6038(void); /* 0xa6038 */

/* game_helpers13.c */
void reset_selection_state_1ad8(void);
uint8_t schedule_allows_2d78(uint8_t owner, uint8_t other, int16_t index, uint32_t time);
uint8_t refresh_selection_3d74(void);
uint8_t slot_matches_unit_7b38(uint8_t group, uint8_t slot, int16_t unit_id);
uint32_t guest_fn_3aa8(uint32_t value); /* 0x3aa8 EAX -> AL */
uint32_t guest_fn_3b1c(uint32_t value); /* 0x3b1c EAX -> AL */

/* game_helpers14.c */
void sort_pairs_946c(void);
uint8_t find_kind8_entry_9e38(ef_addr32 object, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c);
uint32_t read_record_ed80(ef_addr32 dest);
uint32_t text_width_159a0(ef_addr32 text);
void guest_fn_e810(void); /* 0xe810 */
void guest_fn_e89c(void); /* 0xe89c */
uint32_t guest_fn_e980(void); /* 0xe980 */
uint32_t guest_fn_e95c(void); /* 0xe95c */
uint32_t guest_fn_ed30(void); /* 0xed30 AL */

/* game_helpers15.c */
uint32_t find_special_slot_173c4(int16_t owner);
void reset_loop_state_1d318(void);
void build_circle_1fd88(void);
void build_circle_1fe4c(void);
void guest_fn_1a580(void); /* 0x1a580 */
void guest_fn_18a68(void); /* 0x18a68 */
void guest_fn_5331c(void); /* 0x5331c */

/* game_helpers16.c */
void confirm_dialog_277d8(int16_t slot);
void command_linked_units_2b3b0(int16_t owner, uint8_t kind);
void command_unit_and_peers_2b444(int16_t unit_id, uint8_t kind);
uint8_t owner_check_fails_2c6dc(int16_t owner);
void guest_fn_dd370(uint32_t a, uint32_t b, uint32_t c, ef_addr32 d); /* 0xdd370 EAX/EDX/EBX/ECX */
void guest_fn_dc250(void); /* 0xdc250 */
void guest_fn_474b8(int32_t a, int32_t b, uint32_t c); /* 0x474b8 EAX/EDX/EBX */
uint32_t guest_fn_2c0ec(int32_t a); /* 0x2c0ec EAX -> AL */
uint32_t guest_fn_2bbd0(int32_t a); /* 0x2bbd0 EAX -> AX */
uint32_t guest_fn_2bfcc(int32_t a); /* 0x2bfcc EAX -> AL */
uint32_t guest_fn_2c17c(int32_t a); /* 0x2c17c EAX -> AL */

/* game_helpers17.c */
void draw_markers_2f740(void);
uint32_t find_record_at_cell_37b44(int16_t x, int16_t y);
void set_scaled_table_38750(uint16_t a, uint16_t b, uint16_t scale, uint8_t d, uint8_t stack_e, uint8_t stack_f, uint8_t stack_g);
void build_circle_39960(void);
void refresh_selected_unit_40428(void);
uint32_t classify_entry_48ae8(int16_t unit_id, int16_t entry);
void guest_fn_a0ea8(int32_t a, uint32_t b, int32_t c, uint32_t d); /* 0xa0ea8 EAX/EDX/EBX/ECX */
uint32_t guest_fn_cd7f0(int32_t value); /* 0xcd7f0 EAX -> EAX */
void guest_fn_401ac(int32_t owner, uint32_t b, uint32_t c); /* 0x401ac EAX/EDX/EBX */
void guest_fn_cd12e(ef_addr32 format, uint32_t a, uint32_t b); /* 0xcd12e cdecl */

/* game_helpers18.c */
void next_token_50d04(void);
void refresh_owner_entries_544f8(void);
void apply_part_message_54804(ef_addr32 message);
void apply_unit_message_54f4c(ef_addr32 message);
void send_record_state_55aac(ef_addr32 record);
void send_unit_options_56444(int16_t unit_id, uint16_t word_a, uint16_t word_b);
void guest_fn_548e0(int32_t unit); /* 0x548e0 EAX */

/* game_helpers19.c */
void apply_params_message_566b4(ef_addr32 message);
void forward_command_message_572b4(ef_addr32 message);
void send_table_entry_573d0(int16_t index, uint8_t value);
void guest_fn_900c0(int32_t id, uint32_t a, int32_t b); /* 0x900c0 EAX/EDX/EBX */

/* game_helpers20.c */
ef_addr32 find_named_entry_81354(ef_addr32 key);
uint8_t find_row_slot_821f8(uint16_t value, int16_t row);
void store_scaled_vector_831a4(ef_addr32 out, int32_t x, int32_t y, int32_t z);
uint32_t page_row_85c90(int16_t index);
void flag_first_hit_887d0(void);
uint32_t guest_fn_a1a40_key(ef_addr32 key); /* 0xa1a40 EAX -> EAX (older no-argument guest_fn_a1a40 omits the EAX input) */
uint32_t guest_fn_d0470(int32_t a, int32_t x, int32_t y, int32_t z); /* 0xd0470 EAX/EDX/EBX/ECX -> EAX */

/* game_helpers21.c */
uint8_t period_elapsed_89bf0(void);
void draw_nearby_effects_8b988(int32_t x, int32_t y);
void link_effect_8ba30(ef_addr32 effect);
void draw_effects_at_camera_8bb18(void);
uint32_t select_slot_8c930(int16_t page_id);
void draw_record_8e5f0(ef_addr32 record);
void install_handlers_8f9e8(void);
uint32_t guest_fn_8fdf0(int32_t value); /* 0x8fdf0 EAX -> EAX */
void guest_fn_8e664(uint32_t x, uint32_t y, uint32_t z); /* 0x8e664 EAX/EDX/EBX */
void guest_fn_9e094(int32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0x9e094 EAX/EDX/EBX/ECX/stack, ret 4 */

/* game_helpers22.c */
void init_menu_entries_8fa84(void);
void build_path_8fd70(ef_addr32 buffer, ef_addr32 first, ef_addr32 second, ef_addr32 third);
void draw_chain_97464(void);
void init_chain_97a4c(void);
void guest_fn_978e8(ef_addr32 record); /* 0x978e8 EAX */
void guest_fn_974b8(void); /* 0x974b8 */

/* game_helpers23.c */
void refresh_record_2a1c4(ef_addr32 record);
uint32_t sum_owner_weights_2af78(int16_t owner, uint8_t filter);
void draw_cross_3bc60(uint32_t marker, int32_t x, int32_t y);
void draw_box_3bc90(int16_t colour, int32_t x, int32_t y);
void blit_cursor_4d1ec(int32_t x, int32_t y, uint32_t key);
void guest_fn_cec00(int32_t colour, int32_t x1, int32_t y1, int32_t x2, int32_t y2); /* 0xcec00 EAX/EDX/EBX/ECX/stack, ret 4 */

/* tiny_helpers2.c */
void nop_106a8(void);
void nop_151c4(void);
void nop_392fc(void);
void nop_3930c(void);
void nop_54b9c(void);
void nop_68a28(void);
void nop_76944(void);
void nop_80d88(void);
void nop_81864(void);
void nop_866d4(void);
void nop_9be0c(void);
void nop_e7a30(void);
void nop_eb259(void);
void nop_fc8c0(void);
void nop_fd162(void);
void nop_10483c(void);
void nop_d7760(void);
void nop_9d770(void);
void nop_54008(void);
void nop_9d774(uint32_t stack_a, uint32_t stack_b);
void set_flag_151c8(void);
uint32_t call_2dba4_17474(int16_t index);
void set_global_39310(int16_t value);
uint8_t return_true_3ce2c(void);
uint8_t get_byte_3efa0(void);
uint8_t get_byte_4904c(void);
uint8_t return_false_4b188(void);
void set_flag_54efc(void);
void copy_byte_57134(ef_addr32 record);
uint32_t get_flag_7eebc(void);
uint32_t get_flag_810ec(void);
void set_mode_85cf8(void);
void set_global_8bde0(uint32_t value);
uint32_t return_zero_8f3c0(void);
uint8_t return_esc_cc8e0(void);
uint32_t get_global_ce08c(void);
uint32_t abs_cf94a(int32_t value);
void set_word_d76f4(ef_addr32 record, uint16_t value);
uint16_t get_word_d76fc(ef_addr32 record);
void set_word_d7714(ef_addr32 record, uint16_t value);
int32_t get_high_word_d771c(ef_addr32 record);
uint32_t get_dword_d7730(ef_addr32 record);
uint32_t get_dword_d7754(ef_addr32 record);
void set_dword_d7758(ef_addr32 record, uint32_t value);
uint32_t get_dword_d775c(ef_addr32 record);
void set_dword_d7800(ef_addr32 record, uint32_t value);
uint32_t get_global_e2b54(void);
uint32_t get_address_e2fd5(void);
uint32_t get_address_ebd30(void);
uint32_t get_address_ebd36(void);
uint32_t get_global_fa89a(void);
int32_t muldiv_fddb0(int32_t a, int32_t b, int32_t c);
uint32_t return_zero_109e6c(void);
uint32_t return_zero_10d9e0(void);
uint32_t block_start_10d1f4(ef_addr32 block);


/* runtime_helpers10.c */
void update_widget_metrics_a63c0(ef_addr32 widget);
ef_addr32 set_widget_bit6_a73cc(ef_addr32 key, uint32_t unused_edx, uint8_t flag);
void move_widget_a74bc(ef_addr32 widget, int16_t x, int16_t y);
void show_widget_a7588(ef_addr32 widget);
ef_addr32 find_child_by_name_a788c(ef_addr32 widget, ef_addr32 key);
void fill_entries_a8060(void);
uint32_t select_list_node_a9d0c(void);
uint32_t guest_fn_e284c(uint32_t a, uint32_t b); /* 0xe284c EAX/EDX */
void guest_fn_a7510(ef_addr32 widget); /* 0xa7510 EAX */

/* runtime_helpers11.c */
void redraw_widget_ac9a0(ef_addr32 widget);
uint32_t list_has_match_b0554(ef_addr32 a, ef_addr32 b, ef_addr32 list);
void free_record_b1840(ef_addr32 record);
void free_nested_record_b19a0(ef_addr32 record);
void set_table_entry_b4e74(int16_t index, uint8_t byte_b, uint8_t byte_a, uint16_t word_b, uint32_t stack_word_a, ef_addr32 stack_object);
ef_addr32 find_node_b55f8(ef_addr32 list, ef_addr32 a, ef_addr32 b, uint8_t kind);
void guest_fn_ac980(int32_t a, int32_t b, int32_t c, int32_t d); /* 0xac980 EAX/EDX/EBX/ECX */
uint32_t guest_fn_ca9fc(uint32_t a, uint32_t b); /* 0xca9fc EAX/EDX -> AX */
void guest_fn_cc768(ef_addr32 block); /* 0xcc768 EAX */

/* runtime_helpers12.c */
uint32_t restore_list_state_a992c(void);
void clamp_cursor_ac7f0(void);
void update_columns_b0238(ef_addr32 list, int16_t count);
void free_node_list_b18b8(ef_addr32 head, ef_addr32 other);
void free_record_b1918(ef_addr32 record);
ef_addr32 find_child_b4c20(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context);
ef_addr32 find_sibling_b4c7c(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context);
int32_t frame_index_b56d0(int16_t column, int16_t row, ef_addr32 object);
uint32_t count_special_entries_bd508(ef_addr32 record);
uint32_t count_entries_bd554(ef_addr32 record);
ef_addr32 find_target_be438(uint32_t key_a, uint32_t key_b, ef_addr32 list);
uint32_t guest_fn_b0208(ef_addr32 list, uint32_t column, uint32_t field); /* 0xb0208 EAX/EDX/EBX -> AX */
uint32_t guest_fn_cc1ac(uint32_t a, uint32_t b, uint32_t c); /* 0xcc1ac EAX/EDX/EBX -> AX */
uint32_t guest_fn_b549c(int32_t value); /* 0xb549c EAX -> AX */

/* runtime_helpers13.c */
ef_addr32 find_child_b4cdc(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context);
ef_addr32 find_sibling_b4d38(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context);
uint32_t has_free_run_bd73c(ef_addr32 object, int16_t width);
uint32_t record_has_valid_entry_bd86c(ef_addr32 record);
uint32_t nth_match_exists_bd8c8(uint8_t kind, int16_t index, uint8_t sub, ef_addr32 target, ef_addr32 stack_list);
uint32_t count_valid_entries_becc0(ef_addr32 object);
uint32_t guest_fn_cc22c(uint32_t a, uint32_t b, uint32_t c); /* 0xcc22c EAX/EDX/EBX -> AX */
uint32_t guest_fn_bd7cc(ef_addr32 entry, uint32_t kind); /* 0xbd7cc EAX/EDX -> AX */

/* runtime_helpers14.c */
void set_record_links_ca1c8(ef_addr32 record, uint32_t link, uint16_t word_a, uint16_t word_b,
                            uint32_t stack_word_c, uint8_t stack_set_first, uint8_t stack_set_second);
void free_kind2_nodes_ca838(ef_addr32 head, ef_addr32 other);
void translate_bytes_cabb0(ef_addr32 destination, ef_addr32 source);
void copy_matching_fields_cabe8(ef_addr32 list_a, ef_addr32 list_b);
void tick_slot_timers_cb688(ef_addr32 head);
uint32_t guest_fn_cbef4(uint32_t value); /* 0xcbef4 EAX -> AL */

/* runtime_helpers15.c */
void lookup_tiles_cbc54(ef_addr32 grid, ef_addr32 list);
uint32_t node_has_negative_flag_cc468(uint8_t kind, ef_addr32 list);
uint32_t count_open_entries_cc840(ef_addr32 object);
void free_table_blocks_cd7a0(void);
uint32_t forward_word_cd81c(int16_t stack_value);


/* runtime_helpers16.c */
uint32_t claim_device_slot_d70a0(uint8_t priority);
uint32_t add_word_d7704(ef_addr32 record, uint16_t delta);
int32_t scale_by_magnitude_d77dc(int16_t a, int16_t b);
void shift_window_d7bf0(void);


/* game_helpers24.c */
void set_view_origin_dc21c(uint32_t stack_a, uint32_t stack_b, uint32_t stack_y, uint32_t stack_x);
void rotate_point_dc7e4(ef_addr32 out, int16_t a, int16_t b);
void halve_triple_to_fit16_dc890(ef_addr32 a, ef_addr32 b, ef_addr32 c);
uint32_t text_width_4_dced0(ef_addr32 text);
uint32_t text_width_5_dd10c(ef_addr32 text);
uint32_t text_width_6_dd210(ef_addr32 text);
uint32_t text_width_raw_dd650(ef_addr32 text);
void get_view_state_dd988(ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c, ef_addr32 out_d,
                          ef_addr32 stack_e, ef_addr32 stack_f, ef_addr32 stack_g, ef_addr32 stack_h);
uint32_t read_plane_pixel_dec4c(int32_t x, int32_t y);


/* game_helpers25.c */
int32_t read_stream_byte_df21b(ef_addr32 stream);
uint8_t rows_clear_e17b8(uint32_t column, int32_t first_row, int32_t count);
void draw_glyph_e3040(uint32_t color, uint32_t x, uint32_t y, uint32_t glyph);
void draw_text_e30e0(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text);
uint32_t find_word_backwards_e3908(uint32_t value);


/* runtime_helpers17.c */
void select_page_e3c6c(uint32_t stack_page);
void set_settings_e6120(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t stack_e);
void set_page_word_e7c50(int16_t page, uint16_t value, int16_t index);
uint16_t get_page_word_e7c90(int16_t page, int16_t index);
int32_t compare_nocase_eb374(ef_addr32 a, ef_addr32 b, int32_t count);
ef_addr32 bsearch_entries_eb4dc(ef_addr32 key_ptr, ef_addr32 table, int32_t count);


/* game_helpers26.c */
uint32_t assign_page_words_68e84(uint32_t object);
void record_word_7a5f0(uint32_t index);
void match_default_key_aae6c(void);


/* game_helpers26.c */
uint32_t assign_page_words_68e84(uint32_t object);
void record_word_7a5f0(uint32_t index);
void match_default_key_aae6c(void);


/* game_helpers26.c */
uint32_t assign_page_words_68e84(uint32_t object);
void record_word_7a5f0(uint32_t index);
void match_default_key_aae6c(void);
uint32_t guest_fn_68ccc(int16_t object, uint16_t index); /* 0x68ccc AX/DX -> AL */

/* runtime_helpers18.c */
uint32_t find_node_position_aafe8(ef_addr32 out_index, ef_addr32 out_group, ef_addr32 target);
uint32_t alloc_zeroed_ab840(uint32_t size);


/* game_helpers27.c */
void activate_node_65410(uint32_t a, uint32_t b, uint32_t c, uint8_t kind,
                         uint16_t stack_word, uint8_t stack_b, uint8_t stack_c);
void advance_countdown_y_6583c(void);
void advance_countdown_x_658cc(void);
void pick_random_pair_7aa3c(void);
void claim_slot_82168(int16_t owner, int16_t row, uint8_t kind);
int32_t muldiv_saturate_833a0(int32_t a, int32_t b, int32_t c);
void guest_fn_29dd0(int32_t value); /* 0x29dd0 EAX */
void guest_fn_29b14(int32_t value); /* 0x29b14 EAX */

/* game_helpers28.c */
uint8_t all_ready_56ba8(void);
void mark_ready_56cbc(ef_addr32 message);
void mark_ready_56efc(ef_addr32 message);
void forward_slot_message_56f30(ef_addr32 message);
uint8_t all_ready_56ffc(void);
void mark_ready_570fc(ef_addr32 message);
void count_and_forward_577e8(ef_addr32 message);
void push_history_6319c(uint16_t value);
uint8_t history_contains_631fc(uint16_t value);
void count_pending_77748(void);
void set_owner_name_789d0(uint8_t owner, ef_addr32 name);
void latch_mode_word_8b5f0(ef_addr32 pointer);
void step_generator_a45b3(void);
uint32_t find_id_a4640(uint16_t id);
void clear_id_table_a4e6c(void);
void update_mouse_request_a9cd0(void);
void set_pending_name_ac7a8(ef_addr32 name);
uint32_t ratio_lookup_b3ae0(uint32_t a, uint32_t b);
void reset_clip_bounds_c58cc(uint32_t stack_a, uint32_t stack_b);
void load_sincos_dc7bc(uint32_t angle);
void guest_fn_8f22c(int32_t unit, uint32_t index); /* 0x8f22c EAX/EDX */
void guest_fn_57820(ef_addr32 message); /* 0x57820 EAX */

/* game_helpers28.c */
uint8_t all_ready_56ba8(void);
void mark_ready_56cbc(ef_addr32 message);
void mark_ready_56efc(ef_addr32 message);
void forward_slot_message_56f30(ef_addr32 message);
uint8_t all_ready_56ffc(void);
void mark_ready_570fc(ef_addr32 message);
void count_and_forward_577e8(ef_addr32 message);
void push_history_6319c(uint16_t value);
uint8_t history_contains_631fc(uint16_t value);
void count_pending_77748(void);
void set_owner_name_789d0(uint8_t owner, ef_addr32 name);
void latch_mode_word_8b5f0(ef_addr32 pointer);
void step_generator_a45b3(void);
uint32_t find_id_a4640(uint16_t id);
void clear_id_table_a4e6c(void);
void update_mouse_request_a9cd0(void);
void set_pending_name_ac7a8(ef_addr32 name);
uint32_t ratio_lookup_b3ae0(uint32_t a, uint32_t b);
void reset_clip_bounds_c58cc(uint32_t stack_a, uint32_t stack_b);
void load_sincos_dc7bc(uint32_t angle);
void guest_fn_8f22c(int32_t unit, uint32_t index); /* 0x8f22c EAX/EDX */
void guest_fn_57820(ef_addr32 message); /* 0x57820 EAX */

/* game_helpers29.c */
void clear_word_tables_92f0(void);
void send_chat_line_56ccc(void);
void release_pending_73924(int16_t unit_id, int16_t slot_id);
void fill_random_pairs_81868(void);
void raise_gauge_90c2c(int16_t index);
void copy_page_byte_a0b98(int16_t from, int16_t row, int16_t to);
void refresh_flagged_object_a4b08(ef_addr32 object);
int32_t rate_divisor_df2b0(void);
void release_saved_handle_e3b58(void);
uint32_t advance_window_f2141(int32_t count, ef_addr32 window);
void draw_text_f261c(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text);
void draw_text_f29ec(uint32_t color, uint32_t x, uint32_t y, ef_addr32 text);
void remember_request_f41a8(void);
void apply_request_f41c4(void);
void latch_words_f5e6c(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint32_t stack_e, uint32_t stack_f);
void guest_fn_f25b0(uint32_t color, uint32_t x, uint32_t y, uint32_t glyph); /* 0xf25b0 EAX/EDX/EBX/ECX */
void guest_fn_f2960(uint32_t color, uint32_t x, uint32_t y, uint32_t glyph); /* 0xf2960 EAX/EDX/EBX/ECX */

/* game_helpers30.c */
void tick_object_timer_6330(ef_addr32 object);
void tick_countdown_47f48(void);
void copy_row_words_49cd4(ef_addr32 object);
void reset_unit_links_8ccec(int16_t unit_id);
ef_addr32 new_stream_9d114(uint32_t owner);
void rebuild_id_table_a4e14(void);
void tidy_chain_a7204(ef_addr32 object);
void select_draw_handlers_c3a74(void);
uint32_t clear_failed_entry_f8988(ef_addr32 object);
uint32_t count_kind_entries_f89e0(ef_addr32 object);
uint32_t count_filled_entries_f8a30(ef_addr32 object);
uint32_t decrement_last_entry_f8acc(ef_addr32 object);
uint32_t count_set_entries_f8e14(ef_addr32 object);
int32_t has_active_entry_fabf0(ef_addr32 object);
void guest_fn_56a4c(void); /* 0x56a4c */
ef_addr32 guest_fn_9d060(uint32_t size); /* 0x9d060 EAX -> EAX */

/* game_helpers31.c */
uint8_t request_returns_cf_a00(uint32_t argument);
void run_owner_action_15af0(uint32_t slot, uint8_t arg);
void fire_idle_action_1851c(void);
void update_current_mode_2546c(void);
void pop_triple_2a970(void);
void reset_list_view_3711c(void);
void build_ring_table_3a2d0(void);
void probe_cursor_cell_3cd9c(void);
void tick_countdown_47fbc(void);
void cycle_frame_54210(void);
void update_records_5437c(void);
void emit_and_advance_d628(void);
void guest_fn_2b0(ef_addr32 request); /* 0x2b0 EAX */
void guest_fn_15b7c(uint32_t slot, uint32_t extra); /* 0x15b7c EAX/EDX */
uint32_t guest_fn_169b4(uint32_t slot); /* 0x169b4 EAX -> AX */
uint32_t guest_fn_16a04(uint32_t slot); /* 0x16a04 EAX -> AX */
uint32_t guest_fn_16a3c(uint32_t slot); /* 0x16a3c EAX -> AX */
void guest_fn_16034(uint32_t slot, uint32_t extra); /* 0x16034 EAX/EDX */
void guest_fn_161d4(uint32_t slot, uint32_t extra); /* 0x161d4 EAX/EDX */
void guest_fn_16588(uint32_t slot, uint32_t extra); /* 0x16588 EAX/EDX */
void guest_fn_1d420(void); /* 0x1d420 */
void guest_fn_801e4(uint32_t a, uint32_t b, uint32_t c); /* 0x801e4 EAX/EDX/EBX */
uint32_t guest_fn_6aeb4(int32_t index); /* 0x6aeb4 EAX -> AL */
void guest_fn_abe44(ef_addr32 text); /* 0xabe44 EAX */
void guest_fn_a42f4(void); /* 0xa42f4 */
void guest_fn_a4030(void); /* 0xa4030 */
uint32_t guest_fn_7b2f4(uint32_t a, int32_t b, int32_t c); /* 0x7b2f4 EAX/EDX/EBX -> AX */
uint32_t guest_fn_a17e8(void); /* 0xa17e8 -> AX */
void guest_fn_47f9c(void); /* 0x47f9c */
void guest_fn_8f4c(uint32_t frame); /* 0x8f4c EAX */
uint32_t guest_fn_40820(ef_addr32 address); /* 0x40820 EAX -> AL */
void guest_fn_1d700(int32_t index); /* 0x1d700 EAX */
void guest_fn_1e130(void); /* 0x1e130 */
void guest_fn_a78bc_name(ef_addr32 root, ef_addr32 name); /* 0xa78bc EAX/EDX */
void guest_fn_43020(void); /* 0x43020 */

/* game_helpers32.c */
void clear_counters_56208(void);
void run_with_temp_state_5b288(uint8_t a, uint16_t b, uint16_t c, uint8_t d);
void build_ring_table_5b9a8(void);
void build_ring_table_64498(void);
void send_or_defer_748f4(int16_t unit_id, uint32_t b, uint32_t c, uint8_t kind);
void reset_slot_entries_8cae8(ef_addr32 object);
uint32_t next_free_object_9075c(uint32_t start);
void pair_with_target_948c4(int16_t target_id);
void apply_camera_record_97a94(int16_t index);
void read_camera_record_97ca8(ef_addr32 out_x, ef_addr32 out_y, ef_addr32 out_z, ef_addr32 out_a,
                              ef_addr32 stack_b, ef_addr32 stack_c);
void guest_fn_5b2f0(void); /* 0x5b2f0 */
void guest_fn_74970(int32_t unit, uint32_t b, uint32_t c, uint32_t kind); /* 0x74970 EAX/EDX/EBX/ECX */
void guest_fn_8f07c(ef_addr32 object); /* 0x8f07c EAX */
uint32_t guest_fn_906b4(int32_t id); /* 0x906b4 EAX -> AL */

/* game_helpers33.c */
void store_pending_name_57714(void);
void update_flight_62454(void);
void collect_marked_objects_7775c(void);
void install_input_tables_97f90(void);
uint8_t read_stream_char_9d170(ef_addr32 stream);
void guest_fn_625d8(void); /* 0x625d8 */
void guest_fn_6322c(void); /* 0x6322c */
uint32_t guest_fn_d5d18(ef_addr32 name); /* 0xd5d18 EAX -> EAX */
uint32_t guest_fn_9cde8(ef_addr32 buffer, uint32_t count, uint32_t owner); /* 0x9cde8 EAX/EDX/EBX -> EAX */

/* game_helpers34.c */
void unlink_record_9fe1c(int16_t id);
void fill_page_cell_a0ebc(ef_addr32 source);
void apply_page_cell_a2698(int16_t slot, int16_t column);
void accumulate_offsets_a36b0(void);
void remove_id_a4718(uint16_t id);
void append_id_a479c(uint16_t id);
void push_front_id_a4820(uint16_t id);
void guest_fn_a2728(int32_t slot, int32_t a, int32_t b, int32_t c); /* 0xa2728 EAX/EDX/EBX/ECX */

/* game_helpers35.c */
uint8_t has_enemy_link_2c17c(int16_t owner);
void shift_queue_4e6bc(void);
void send_owner_summary_56a4c(void);
void update_hud_79128(void);
void init_name_entries_80a18(void);
void release_object_878c8(ef_addr32 object);
void update_nearest_link_90880(ef_addr32 object, ef_addr32 candidate);
void guest_fn_78848(void); /* 0x78848 */
void guest_fn_77c34(void); /* 0x77c34 */
void guest_fn_77c90(void); /* 0x77c90 */
void guest_fn_78934(void); /* 0x78934 */
void guest_fn_78a0c(void); /* 0x78a0c */
void guest_fn_78ac0(void); /* 0x78ac0 */
void guest_fn_78b4c(void); /* 0x78b4c */
void guest_fn_78f3c(void); /* 0x78f3c */
void guest_fn_a1770(void); /* 0xa1770 */

/* game_helpers36.c */
void classify_angle_92070(void);
void select_page_layout_9dfc0(int16_t page, uint16_t b, uint16_t c, uint16_t d);
void stamp_page_cell_a1424(int16_t column, int16_t value, uint32_t code, uint8_t flag);
void latch_order_target_a905c(ef_addr32 message);
void notify_group_links_ab0dc(uint32_t target);
void guest_fn_a0c04(int32_t column, int32_t row); /* 0xa0c04 EAX/EDX */
void guest_fn_ab0b0(uint32_t target, uint32_t link); /* 0xab0b0 EAX/EDX */
void guest_fn_ab060(uint32_t target); /* 0xab060 EAX */

/* game_helpers37.c */
void draw_text_4_dccd8(uint8_t color, int32_t x, uint16_t y, ef_addr32 text);
void draw_text_5_dd1b0(uint8_t color, int32_t x, uint16_t y, ef_addr32 text);
uint32_t text_width_17_dd460(ef_addr32 text);
uint32_t text_width_5_dd55c(ef_addr32 text);
uint32_t text_width_6_dd728(ef_addr32 text);
void init_time_constants_e7850(void);
void interpolate_line_e85f0(ef_addr32 out_x, ef_addr32 out_y);
uint32_t guest_fn_dcc98(uint32_t glyph, int32_t x, int32_t y, uint32_t color); /* 0xdcc98 AL/EDX/EBX/ECX -> AL */
uint32_t guest_fn_dd164(uint32_t glyph, int32_t x, int32_t y, uint32_t color); /* 0xdd164 AL/EDX/EBX/ECX -> AL */

/* game_helpers38.c */
void shift_history_46070(void);
void update_timers_48064(void);
void share_link_48798(int16_t first, int16_t second, uint8_t mode);
void share_link_48848(int16_t first, int16_t second, uint8_t mode);
void reset_flight_state_54010(void);
void reset_unit_teams_54410(void);
void apply_owner_summary_56af4(ef_addr32 message);
uint8_t turn_direction_5f588(ef_addr32 a, ef_addr32 b, ef_addr32 c);
uint32_t guest_fn_7574c(int32_t value); /* 0x7574c EAX -> AL */

/* game_helpers39.c */
void rotate_vector_6456c(ef_addr32 vector);
void rotate_vector_64618(ef_addr32 vector);
uint32_t find_next_unit_663a4(uint32_t start, uint8_t mode);
void transform_point_69680(ef_addr32 out_xyz, ef_addr32 out_base, int16_t x, int16_t y);
void blend_palette_758c0(uint16_t factor, ef_addr32 source, ef_addr32 dest, uint8_t red,
                         uint8_t stack_green, uint8_t stack_blue);
void trigger_linked_event_77534(uint32_t id);


/* tiny_helpers3.c */
void reset_handler_1d420(void);
void set_limit_768fc(void);
void set_timer_77c28(void);
void set_handler_9854c(void);
void reseed_generator_a45a8(void);
void set_pointer_e3c60(void);
void set_limit_eb24e(void);
void set_pointer_fd371(void);


/* game_helpers40.c */
void step_damage_level_6322c(void);
void reset_target_state_65960(void);
uint8_t is_valid_target_6a82c(int16_t first, int16_t second);
void fade_palette_76948(void);
void report_formation_76f58(uint32_t roster, ef_addr32 formation);
void guest_fn_76b30(uint32_t roster, uint32_t slot, uint32_t value); /* 0x76b30 EAX/EDX/EBX */

/* game_helpers41.c */
void select_chord_mode_5488(void);
void count_down_slots_16d60(ef_addr32 base, uint8_t kind, int16_t amount);
void latch_input_state_1c3d0(void);
ef_addr32 guest_fn_a7ec4_key(ef_addr32 text, uint32_t key); /* 0xa7ec4 EAX/EDX */

/* game_helpers42.c */
void select_mode_from_record_45a4(void);
void reset_menu_state_4738(void);
uint32_t guest_fn_105a4(ef_addr32 a, ef_addr32 b, ef_addr32 c, ef_addr32 d, ef_addr32 name); /* 0x105a4 EAX/EDX/EBX/ECX + stack */

/* game_helpers43.c */
void check_data_file_283c8(void);
void draw_debug_counters_404bc(void);
void guest_fn_275a4(void); /* 0x275a4 */
uint32_t guest_fn_27fec(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x27fec EAX/EDX/EBX/ECX */
void guest_fn_a4342_plain(ef_addr32 destination, ef_addr32 format); /* 0xa4342 cdecl */
uint32_t guest_fn_a7900(ef_addr32 name); /* 0xa7900 cdecl */
void guest_fn_48c90(uint32_t row, uint32_t col, uint32_t y, ef_addr32 text); /* 0x48c90 cdecl */

/* runtime_helpers19.c */
uint32_t dos_call_350b_880(void);
void dos_call_2505_a9c(uint32_t first, uint32_t second);
void dos_call_3502_c74(int16_t value);
uint32_t open_checked_cac(uint32_t name, uint32_t mode);
void alloc_zero_5x_6df0(uint8_t count);
uint32_t alloc_zero_product_7780(int32_t count, int32_t size);
uint32_t parse_enum_ec3c(void);
uint32_t parse_enum_eea0(void);
uint32_t parse_enum_ef98(void);
uint32_t parse_enum_f0ec(void);
uint32_t read_record_list_f038(ef_addr32 list);
uint32_t read_group_list_f08c(ef_addr32 list);
uint32_t guest_fn_9c2d3(uint32_t name, uint32_t mode); /* 0x9c2d3 cdecl */
void guest_fn_efe8(ef_addr32 entry); /* 0xefe8 EAX */

/* runtime_helpers20.c */
void draw_centered_entries_27544(ef_addr32 list, int16_t y, int32_t step);
void alloc_unit_table_2aef8(int16_t count);
void run_progress_delay_34d50(void);
void arm_timer_record_35748(uint8_t kind, uint16_t duration, uint8_t flag);
void alloc_record_table_372d4(int16_t count);
uint32_t take_free_slot_38584(void);
void alloc_work_area_3c410(void);
void register_string_3c6c0(ef_addr32 text);
ef_addr32 find_entry_or_abort_3c730(ef_addr32 name, int16_t page);
ef_addr32 duplicate_string_3ccec(ef_addr32 text);
void alloc_table_25_76b10(uint8_t count);
void clear_buffer_7a3e8(void);
ef_addr32 make_record_17_82000(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint8_t e);
ef_addr32 make_record_16_82024(uint32_t a, uint32_t b, uint32_t unused_bx, uint32_t unused_cx,
                               uint32_t c, uint32_t d);
void set_table_value_8c0c8(int16_t index, uint32_t value);
void guest_fn_a3eac_args(uint32_t a, uint32_t b); /* 0xa3eac EAX/EDX */
uint32_t guest_fn_e2a94(uint32_t size); /* 0xe2a94 EAX -> EAX */
void guest_fn_3c5b8(void); /* 0x3c5b8 */

/* runtime_helpers21.c */
ef_addr32 find_entry_by_id_8fdf0(int16_t id);
void init_buffer_8fee0(void);
ef_addr32 alloc_zero_68_8ff20(uint8_t count);
void reset_sound_slot_980e8(uint8_t slot);
void start_transfer_985b0(uint8_t mode, uint32_t target, uint32_t start, uint32_t end);
ef_addr32 alloc_or_abort_9d060(uint32_t size);
uint32_t countdown_record_a00a4(ef_addr32 record);
void read_checked_a1afc(uint32_t buffer, uint32_t count);
void write_checked_a1b28(uint32_t buffer, uint32_t count);
uint8_t check_count_a2900(uint16_t count);
void check_name_a39ec(void);
void cache_current_slot_a3bdc(void);
void shutdown_device_a3da0(void);
void close_device_a3e04(void);
void issue_request_a3e50(uint32_t request, uint16_t size, uint8_t flag);
void issue_request_ready_a3eac(uint32_t request, uint16_t size);
void poll_device_a3fe0(void);
void start_device_a42d0(void);
void stop_device_a42f4(void);
ef_addr32 find_named_or_abort_a46cc(ef_addr32 name);
void guest_fatal_arg(ef_addr32 format, uint32_t value); /* [0x132a74] cdecl, one argument */
void guest_fn_d5ec0(ef_addr32 slot); /* 0xd5ec0 EAX */
uint32_t guest_fn_a79d4_ret(uint32_t a, uint32_t b, uint32_t c); /* 0xa79d4 stack -> EAX */
uint32_t guest_fn_a7a1c_ret(uint32_t a, uint32_t b, uint32_t c); /* 0xa7a1c stack -> EAX */
void guest_fn_a42f4(void); /* 0xa42f4 */
void guest_fn_a3fe0(void); /* 0xa3fe0 */
uint32_t guest_fn_a292c(void); /* 0xa292c -> AL */
ef_addr32 guest_fn_a4678(ef_addr32 name); /* 0xa4678 EAX -> EAX */

/* runtime_helpers22.c */
void set_object_name_a75cc(ef_addr32 object, ef_addr32 name);
ef_addr32 find_widget_checked_a78bc(ef_addr32 root, ef_addr32 name);
void close_handle_a7990(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_handle);
uint32_t read_handle_a79d4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                           uint32_t stack_handle, uint32_t stack_count, uint32_t stack_buffer);
uint32_t write_handle_a7a1c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                            uint32_t stack_handle, uint32_t stack_count, uint32_t stack_buffer);
void seek_handle_a7a64(uint32_t first, uint32_t second, uint32_t handle);
int32_t clock_delta_a7d40(void);
void run_entry_callbacks_abde4(void);
void report_fatal_ac7c8(uint32_t value);
uint8_t write_whole_file_b3d00(ef_addr32 name, uint32_t buffer, uint32_t count);
uint32_t lookup_id_b3f58(int16_t id);
void alloc_work_block_c32dc(int32_t size);
uint32_t start_driver_c34cc(uint32_t request);
ef_addr32 guest_fn_a788c(ef_addr32 root, ef_addr32 name); /* 0xa788c EAX/EDX */
void guest_fn_bc421(uint32_t a, uint32_t b, uint32_t c); /* 0xbc421 EAX/EDX/EBX */
uint32_t guest_fn_f1d90(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xf1d90 EAX/EDX/EBX/ECX */
uint32_t guest_fn_f1f5a(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xf1f5a EAX/EDX/EBX/ECX */
void guest_fn_f2183(uint32_t a, uint32_t b, uint32_t c); /* 0xf2183 EAX/EDX/EBX */
void guest_fn_f227f(uint32_t a); /* 0xf227f EAX */
void guest_fn_de648(uint32_t a, uint32_t b); /* 0xde648 EAX/EDX */
uint32_t guest_fn_a7948(ef_addr32 name); /* 0xa7948 cdecl */
uint32_t guest_fn_e2b54(void); /* 0xe2b54 -> EAX */
uint32_t guest_fn_c3430(uint32_t a); /* 0xc3430 EAX -> AL */
void guest_fn_c3328(void); /* 0xc3328 */

/* runtime_helpers23.c */
uint8_t query_driver_words_c3714(uint32_t request, ef_addr32 first, ef_addr32 second);
void blit_screen_c3f1c(void);
uint32_t find_name_index_ca780(ef_addr32 name);
uint32_t alloc_checked_cc744(uint32_t size);
void guest_fn_c3a28(void); /* 0xc3a28 */
uint32_t guest_fn_9d524(ef_addr32 a, ef_addr32 b); /* 0x9d524 EAX/EDX */
uint32_t guest_fn_c3f78(uint32_t size); /* 0xc3f78 EAX */

/* runtime_helpers24.c */
void store_offset_cd2a8(uint32_t pointer);
void ensure_text_block_cd640(int16_t slot);
void copy_text_to_slot_cd67c(int16_t slot, ef_addr32 text);
void set_text_slot_cd724(int16_t slot, ef_addr32 text);
uint32_t add_text_slot_cd754(ef_addr32 text);
uint32_t get_text_slot_cd7f0(int16_t slot);
void write_log_d16d0(uint32_t buffer, uint32_t count);
void open_log_d1774(ef_addr32 name);
void set_mode_640x400_d1da8(uint32_t first, uint32_t second);
void set_video_mode_d1de8(uint32_t mode, uint32_t mode_id, uint32_t height, uint32_t width);
void alloc_object_text_d6fa8(ef_addr32 object);
void channel_call_102858_d73b8(uint8_t channel);
void channel_call_102824_d7458(uint8_t channel);
void channel_call_1027a0_d7490(uint8_t channel);
void channel_start_d74d0(uint8_t channel);
void channel_call_1027cc_d7530(uint8_t channel);
void run_named_handler_da190(ef_addr32 table, ef_addr32 name);
void toggle_page_dc250(void);
void cycle_page_dc280(void);
void guest_fn_eb35c(void); /* 0xeb35c */
void guest_fn_fc534(void); /* 0xfc534 */
uint32_t guest_fn_fc600(uint32_t mode); /* 0xfc600 EAX -> AL */
void guest_fn_d1d40(uint32_t width, uint32_t height); /* 0xd1d40 EAX/EDX */
void guest_fn_d16fc(ef_addr32 text); /* 0xd16fc EAX */
void guest_fn_d170c(uint32_t value); /* 0xd170c EAX */
uint32_t guest_fn_d7070(void); /* 0xd7070 -> EAX */
void guest_fn_104920(uint32_t a, uint32_t b); /* 0x104920 cdecl */
void guest_fn_1027f8(uint32_t a); /* 0x1027f8 cdecl */
void guest_fn_1027a0(uint32_t a); /* 0x1027a0 cdecl */
void guest_fn_1027cc(uint32_t a); /* 0x1027cc cdecl */
void guest_fn_102824(uint32_t a); /* 0x102824 cdecl */
void guest_fn_102858(uint32_t a); /* 0x102858 cdecl */

/* runtime_helpers25.c */
uint32_t find_table_entry_dead0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_key);
void wait_ticks_or_key_e1130(uint32_t ticks);
void wait_ticks_or_press_e115c(uint32_t ticks);
void init_arena_e29a0(void);
uint32_t arena_take_e29e8(uint32_t bytes);
uint32_t arena_alloc_e2a94(uint32_t bytes);
void arena_free_e2af4(uint32_t block);
void read_resource_e3170(uint32_t buffer, uint16_t count);
ef_addr32 next_pool_record_e3c20(void);
void append_translated_e3fbc(uint32_t value);
void release_buffer_e9574(void);
uint32_t alloc_zeroed_checked_ea100(uint32_t size);
void read_chunk_header_ea1a0(ef_addr32 object, uint32_t source);
void decode_chunk_ea420(uint32_t destination);
void install_exit_handler_ea470(uint32_t mode);
void open_data_file_eaab0(void);
void read_data_block_eaaf4(void);
void guest_fn_e2a1c(void); /* 0xe2a1c */
uint32_t guest_fn_1092b0(ef_addr32 pointer); /* 0x1092b0 EAX */
void guest_fn_9cc28(uint32_t a, uint32_t b); /* 0x9cc28 EAX/EDX */
uint32_t guest_fn_10da50(ef_addr32 a, uint32_t b); /* 0x10da50 cdecl -> AL */
void guest_fn_10dc7f(uint32_t a, uint32_t b); /* 0x10dc7f EAX/EDX */
void guest_fn_81c(void); /* 0x81c */
uint32_t guest_fn_9ccb8(ef_addr32 name); /* 0x9ccb8 EAX -> EAX */

/* runtime_helpers26.c */
void build_data_path_eabe8(ef_addr32 name);
uint32_t alloc_block_checked_eb4a0(uint32_t size);
void select_entry_kind_f1850(ef_addr32 entry);
void restart_driver_f2dfc(void);
void guest_fn_f2cd0(void); /* 0xf2cd0 */
void guest_fn_f2dcc(ef_addr32 path); /* 0xf2dcc EAX */
uint32_t guest_fn_c34cc_ret(uint32_t request); /* 0xc34cc EAX -> EAX */
void guest_fn_c3f80(uint32_t block); /* 0xc3f80 EAX */

/* runtime_helpers27.c */
uint32_t alloc_aligned_510(uint32_t size);
uint32_t start_request_628(uint32_t first, uint32_t second, int32_t third);
void finish_request_6b8(uint32_t index);
uint32_t start_timer_734(uint32_t source, uint32_t unused_dx, uint32_t flags);
void copy_pending_name_6b50(void);
uint32_t read_header_list_f198(void);
uint32_t read_record_header_f604(ef_addr32 record);
uint32_t read_point_list_fb10(ef_addr32 list);
uint32_t read_polyline_list_fbcc(ef_addr32 record);
uint32_t read_group_header_fc54(ef_addr32 group);
ef_addr32 find_unique_node_ffb4(ef_addr32 name, ef_addr32 head);
uint32_t guest_fn_9be38(uint32_t size); /* 0x9be38 cdecl */
uint32_t guest_fn_9c220(uint32_t a, uint32_t b, uint32_t c); /* 0x9c220 cdecl */
uint32_t guest_fn_9c23d(uint32_t a, uint32_t b, uint32_t c); /* 0x9c23d cdecl */
uint32_t guest_fn_324(uint32_t a); /* 0x324 EAX */
uint64_t guest_fn_348(uint32_t a); /* 0x348 EAX -> EDX:EAX */
void guest_fn_484(uint32_t a, uint32_t b, uint32_t c); /* 0x484 EAX/EDX/EBX */
void guest_fn_b3c04(uint32_t a, uint32_t b); /* 0xb3c04 EAX/EDX */
void guest_fn_f13c(ef_addr32 entry); /* 0xf13c EAX */
void guest_fn_f314(ef_addr32 entry, uint32_t value); /* 0xf314 EAX/EDX */
void guest_fn_ed80(ef_addr32 point); /* 0xed80 EAX */

/* runtime_helpers28.c */
uint32_t read_four_words_105a4(ef_addr32 first, ef_addr32 second, ef_addr32 third, ef_addr32 fourth,
                               ef_addr32 stack_name);
void reroll_list_entries_1e3c4(void);
void write_save_header_285b8(void);
void read_save_header_2863c(void);
void start_session_34ca4(void);
void clear_session_34ea4(void);
void end_session_34e18(void);
void open_stream_36054(ef_addr32 name, uint8_t volume);
uint32_t take_table_record_3730c(void);
void claim_unit_slot_39e0c(int16_t unit_id);
uint32_t guest_fn_cd150(int32_t cell); /* 0xcd150 EAX */
uint32_t guest_fn_1e214(int32_t a, int32_t b, int32_t c, int32_t d); /* 0x1e214 EAX/EDX/EBX/ECX */
uint32_t guest_fn_cd258(void); /* 0xcd258 -> AX */
void guest_fn_ab324(uint32_t a, uint32_t b); /* 0xab324 EAX/EDX */
void guest_fn_ab47c(uint32_t a); /* 0xab47c EAX */
uint32_t guest_fn_a29cc(uint32_t a); /* 0xa29cc EAX -> AL */
void guest_fn_a29f8(void); /* 0xa29f8 */
void guest_fn_a2bdc(void); /* 0xa2bdc */
void guest_fn_a41d0(void); /* 0xa41d0 */
uint32_t guest_fn_e1780(void); /* 0xe1780 -> AL */
uint32_t guest_fn_e1744(uint32_t a); /* 0xe1744 EAX -> AL */
void guest_fn_a3da0(void); /* 0xa3da0 */
void guest_fn_a3e04(void); /* 0xa3e04 */
void guest_fn_35fbc(void); /* 0x35fbc */
void guest_fn_9ce84(uint32_t handle); /* 0x9ce84 EAX */

/* runtime_helpers29.c */
void resolve_entry_cells_3ce70(void);
void run_matching_handler_f478c(void);
void store_range_fddf0(ef_addr32 center, uint32_t radius);
uint32_t guest_fn_8b9e8(uint32_t a, uint32_t b); /* 0x8b9e8 EAX/EDX -> AX */

/* game_helpers44.c */
void seek_segment_49d2c(ef_addr32 list);
uint16_t grid_cell_word_49f28(int16_t a, int16_t b, int16_t c);
void locate_segment_4b0d0(ef_addr32 list);
uint8_t dispatch_command_50f54(uint8_t mask);
void skip_to_word_50fcc(ef_addr32 name);
void guest_fatal_arg3(ef_addr32 format, uint32_t a, uint32_t b, uint32_t c); /* [0x132a74] cdecl, three arguments */
void guest_fn_50ca4(void); /* 0x50ca4 */
void guest_fn_50d04(void); /* 0x50d04 */

/* game_helpers45.c */
void finish_marker_round_558b0(void);
void announce_ready_56bdc(void);
void announce_ready_57030(void);
void init_status_text_7a4a4(void);
void set_session_text_7c4fc(ef_addr32 name, uint32_t target);
uint8_t claim_players_7e5a0(void);
uint8_t sync_players_7e758(void);
void wait_round_clear_7e7e0(void);
uint32_t guest_fn_56ba8(void); /* 0x56ba8 -> AL */
uint32_t guest_fn_56ffc(void); /* 0x56ffc -> AL */
void guest_fn_e2988(uint32_t ticks); /* 0xe2988 EAX */
void guest_fn_581b0(void); /* 0x581b0 */
void guest_fn_58094(void); /* 0x58094 */
void guest_fn_9c980_sfx(ef_addr32 buffer, ef_addr32 suffix); /* 0x9c980 EAX/EDX */
void guest_fn_e1e6c_3(uint32_t a, uint32_t b, uint32_t c); /* 0xe1e6c EAX/EDX/EBX */
uint32_t guest_fn_7e670(void); /* 0x7e670 -> AL */

/* game_helpers46.c */
void wait_target_count_7eccc(void);
void init_sound_bank_80058(void);
void init_sound_bank_81230(void);
void alloc_grid_table_82060(int16_t rows);
void rewrite_settings_830b8(void);
uint8_t project_point_83424(int16_t x, int16_t y, uint32_t scale, ef_addr32 out_x, ef_addr32 out_y);
void guest_fn_7eda4(void); /* 0x7eda4 */
uint32_t guest_fn_e94ec(ef_addr32 path, uint32_t flags); /* 0xe94ec EAX/EDX -> AL */
void guest_fn_e93dc(uint32_t a, uint32_t b); /* 0xe93dc EAX/EDX */
void guest_fn_d6f90(uint32_t a, uint32_t b, uint32_t c); /* 0xd6f90 EAX/EDX/ECX */
uint32_t guest_fn_d7764_7(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s1, uint32_t s2, uint32_t s3); /* 0xd7764 EAX/EDX/EBX/ECX + stack */
void guest_fn_d7758(uint32_t a, uint32_t b); /* 0xd7758 EAX/EDX */

/* game_helpers47.c */
void center_view_842b4(void);
void reset_unit_view_846c8(ef_addr32 unit);
void dispatch_named_handler_84cfc(int16_t count, ef_addr32 args);
void fill_unit_entry_84ff0(int16_t unit, ef_addr32 position, uint32_t u2, uint32_t u3,
                           uint32_t stack_a, uint32_t stack_b, uint32_t stack_index, uint32_t stack_flag,
                           ef_addr32 stack_words);
void alloc_unit_entries_850a4(int16_t unit, uint8_t count);
uint16_t find_next_slot_8600c(uint8_t kind);
void guest_fn_dc91c(ef_addr32 out, ef_addr32 a, ef_addr32 b); /* 0xdc91c EAX/EDX/EBX */
void guest_fn_91afc(int32_t a, uint32_t b); /* 0x91afc EAX/EDX */
void guest_fn_c4654(uint32_t a, uint32_t b, uint32_t c); /* 0xc4654 cdecl */

/* game_helpers48.c */
void step_object_883cc(ef_addr32 object);
void step_objects_8846c(void);
void set_unit_speed_8cc40(int16_t unit_id);
void check_cell_trigger_8d030(void);
void guest_fn_55aac(ef_addr32 object); /* 0x55aac EAX */
void guest_fn_89e08(ef_addr32 object); /* 0x89e08 EAX */
void guest_fn_88134(ef_addr32 object); /* 0x88134 EAX */
void guest_fn_88574(ef_addr32 object); /* 0x88574 EAX */
uint32_t guest_fn_237c8(ef_addr32 point); /* 0x237c8 EAX */
void guest_fn_80b14(ef_addr32 record); /* 0x80b14 EAX */
uint32_t guest_fn_7b2f4_4(int32_t a, int32_t b, int32_t c, uint32_t d); /* 0x7b2f4 EAX/EDX/EBX/ECX */
void guest_fn_8cd78(int32_t unit); /* 0x8cd78 EAX */

/* game_helpers49.c */
void track_closer_object_8f3f4(ef_addr32 object, ef_addr32 other);
void publish_entry_position_8f930(void);
void fade_effect_slots_8fc24(void);
void name_formation_units_8fcec(int16_t player);
void load_record_table_8fe58(void);
void guest_fn_8fd70(ef_addr32 buffer, ef_addr32 a, ef_addr32 b); /* 0x8fd70 EAX/EDX/EBX */
uint32_t guest_fn_8fdb8(ef_addr32 record); /* 0x8fdb8 EAX -> AX */
uint32_t guest_fn_81354(ef_addr32 text); /* 0x81354 EAX */

/* game_helpers50.c */
uint16_t create_effect_record_8ff48(void);
void apply_slot_result_90124(int16_t slot, int16_t kind, uint16_t extra);
void drop_object_near_camera_90378(ef_addr32 object);
void reset_angle_limits_93f44(void);
void update_navigation_target_94954(int16_t unit_id);
uint32_t guest_fn_cd2e8_ret(int32_t value); /* 0xcd2e8 EAX -> EAX */
void guest_fn_10a9c(int32_t a, int32_t b); /* 0x10a9c EAX/EDX */
void guest_fn_55f0c(int32_t a, int32_t b, int32_t c, int32_t d); /* 0x55f0c EAX/EDX/EBX/ECX */
void guest_fn_d9cb4(ef_addr32 a, ef_addr32 b, ef_addr32 c); /* 0xd9cb4 EAX/EDX/EBX */

/* runtime_helpers30.c */
void alloc_slot_table_39db0(uint8_t count);
ef_addr32 make_node_8204c(uint32_t value);
uint8_t unsupported_84650(void);
uint8_t require_device_ready_a292c(void);
void run_idle_callback_a41a0(void);
void remember_clock_a7d34(void);
void run_word_callback_ab820(uint32_t value);
void wait_ticks_e1110(uint32_t ticks);
uint8_t unsupported_query_e1730(void);
uint8_t unsupported_query_e1744(void);
uint8_t unsupported_query_e1758(void);
uint8_t unsupported_query_e1780(void);
void fatal_with_arg_e1948(uint32_t value);
void switch_page_eb25a(uint32_t page);
void switch_page_eb272(uint32_t page);
void check_error_flag_eb35c(void);
void forward_callback_ed6c5(void);
void guest_fn_20d(uint32_t value); /* 0x20d EAX */
void switch_page_eb272(uint32_t page);

/* runtime_helpers31.c */
void select_sound_slot_98050(uint8_t slot);
void update_records_9a624(void);
void count_event_9aa78(ef_addr32 record, int16_t kind);
void read_text_line_9d1e8(ef_addr32 stream);
void load_string_table_9d990(void);
uint32_t guest_fn_d5e0c(ef_addr32 slot); /* 0xd5e0c EAX -> EAX */
void guest_fn_82e40(void); /* 0x82e40 */
uint32_t guest_fn_9a6fc(ef_addr32 record); /* 0x9a6fc EAX -> EAX */
void guest_fn_9a8b4(ef_addr32 record, int32_t value); /* 0x9a8b4 EAX/EDX */
void guest_fn_9a998(ef_addr32 record); /* 0x9a998 EAX */
void guest_fn_9b780(void); /* 0x9b780 */
void guest_fn_9b8a0(void); /* 0x9b8a0 */
uint32_t guest_fn_9d964(ef_addr32 text); /* 0x9d964 EAX -> AX */
void guest_fatal_arg0(ef_addr32 format); /* [0x132a74] cdecl, no extra argument */
void count_event_9aa78(ef_addr32 record, int16_t kind);

/* game_helpers51.c */
void create_state_file_a1b54(ef_addr32 name, uint16_t version);
void open_state_file_a1bf4(ef_addr32 name, uint16_t version);
void blend_linear_a2188(ef_addr32 key);
void blend_quadratic_a2434(ef_addr32 key);
void append_packet_a3f74(ef_addr32 bytes, uint16_t size);
void update_frame_delta_a4138(void);
uint8_t start_device_a41dc(void);
void activate_widget_a4cb4(uint16_t index);
void guest_fn_a1cb4(void); /* 0xa1cb4 */
void guest_fn_a4104(void); /* 0xa4104 */
uint32_t guest_fn_a2f70(void); /* 0xa2f70 -> AL */

/* runtime_helpers32.c */
void draw_widget_text_a6c20(ef_addr32 widget);
void draw_widget_frame_a713c(ef_addr32 widget);
void set_record_name_a7510(ef_addr32 record, ef_addr32 name);
ef_addr32 lookup_key_a7ec4(ef_addr32 name);
ef_addr32 find_key_checked_a7f64(ef_addr32 name);
void reset_key_tables_a9bfc(void);
void resolve_scene_groups_aa350(void);
void guest_fn_e26bc(uint32_t a, int32_t x, int32_t y, ef_addr32 text, uint32_t color); /* 0xe26bc */
void guest_fn_c92e4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, uint32_t color); /* 0xc92e4 */
uint16_t guest_fn_aa2f8(ef_addr32 node); /* 0xaa2f8 */
void guest_fn_aa198(ef_addr32 node); /* 0xaa198 */
void guest_fn_aa1c0(ef_addr32 node); /* 0xaa1c0 */

/* runtime_helpers33.c */
void patch_group_pointers_aa4d4(void);
void visit_group_nodes_ab28c(ef_addr32 context);
void select_default_video_aca14(void);
void select_video_mode_acac0(ef_addr32 name);
void clear_ui_state_ac16c(void);
void reset_ui_state_ac0e0(void);
void run_page_callbacks_ac240(ef_addr32 arg, uint8_t flag);
ef_addr32 alloc_record_pool_b0138(ef_addr32 listp, ef_addr32 countp);
void free_record_tree_b1a68(ef_addr32 rec);
void print_record_b21ec(ef_addr32 rec, uint32_t out);
void dump_table_a_b2810(ef_addr32 rec, uint32_t out);
void dump_table_b_b28b8(ef_addr32 rec, uint32_t out);
void dump_group_list_b29a8(ef_addr32 list, uint32_t out);
uint32_t guest_fn_aa4b8(int32_t value, ef_addr32 group); /* 0xaa4b8 */
void guest_fn_ab260(ef_addr32 context, ef_addr32 pointer); /* 0xab260 */
void guest_fn_ab174(ef_addr32 context); /* 0xab174 */
uint8_t guest_fn_e4304(void); /* 0xe4304 -> AL */
void guest_fn_e4264(void); /* 0xe4264 */
void guest_fn_e4188(void); /* 0xe4188 */
void guest_fn_ab8b0(void); /* 0xab8b0 */
void guest_fn_ca200(ef_addr32 name); /* 0xca200 */
void guest_fn_e17f8(uint32_t a, uint32_t b); /* 0xe17f8 */
uint32_t guest_fn_ab8c4(void); /* 0xab8c4 -> EAX */
void guest_fn_ca5e4(uint32_t a, uint32_t b); /* 0xca5e4 */
void guest_fn_ac2e4(ef_addr32 a, uint32_t b); /* 0xac2e4 */
void guest_fn_a5abc(void); /* 0xa5abc */
uint32_t guest_fn_cc6c0(uint32_t size, ef_addr32 tag, uint32_t size2, uint32_t count); /* 0xcc6c0 cdecl */
void guest_fn_b1bb0(uint32_t handle, ef_addr32 format, ...); /* 0xb1bb0 cdecl, variadic */
void guest_fn_b2a2c(ef_addr32 pointer, uint32_t handle); /* 0xb2a2c */
void guest_fn_b2504(ef_addr32 pointer, uint32_t index, uint32_t handle); /* 0xb2504 */
void guest_fn_b2960(ef_addr32 pointer, uint32_t handle); /* 0xb2960 */

/* runtime_helpers34.c */
void print_point_b2d8c(ef_addr32 rec, uint32_t out);
void print_object_b2e98(ef_addr32 rec, uint32_t out);
uint32_t read_file_payload_size_b3b90(ef_addr32 name);
uint32_t take_pool_block_b3db0(uint32_t size);
uint32_t parse_open_mode_bc1d3(ef_addr32 mode);
uint8_t clear_unusable_slots_bd5f8(ef_addr32 rec);
void link_matching_entries_be320(ef_addr32 listp, ef_addr32 otherp);
uint32_t guest_fn_b2e64(ef_addr32 record); /* 0xb2e64 -> AX */
uint32_t guest_fn_a7ae0(uint32_t handle, uint32_t flag); /* 0xa7ae0 stack */
void guest_fn_ebe6d(uint32_t code); /* 0xebe6d EAX */
uint32_t guest_fn_bdbc8(ef_addr32 record); /* 0xbdbc8 -> AX */
uint32_t guest_fn_bd5a8(ef_addr32 record); /* 0xbd5a8 -> AX */
uint32_t guest_fn_bb7c0(ef_addr32 entry); /* 0xbb7c0 -> AX */

/* runtime_helpers35.c */
void init_video_d1d40(uint32_t width, uint32_t height);
void fill_dwords_deb87(ef_addr32 dst, uint32_t value, uint32_t count);
uint32_t text_width_e284c(ef_addr32 text, uint16_t font);
void build_rotation_e994c(uint32_t flags, uint32_t a, uint32_t b, uint32_t c);
void transform_vector_e99b8(ef_addr32 out, int32_t x, int32_t y, int32_t z);
int32_t set_file_buffer_f22af(ef_addr32 file, ef_addr32 buffer, uint32_t mode, uint32_t size);
void draw_glyph_f25b0(uint8_t value, uint32_t x, uint32_t row, uint32_t glyph);
void step_animation_f36a4(void);
uint32_t count_matching_entries_fba6c(uint32_t a, uint32_t b, ef_addr32 listp);
uint32_t guest_fn_fc6c8(void); /* 0xfc6c8 -> EAX */
void guest_fn_fc39c(uint32_t a, uint32_t b, uint32_t width, uint32_t height); /* 0xfc39c EAX/EDX/EBX/ECX */
void guest_fn_c3ae8(void); /* 0xc3ae8 */
void guest_fn_fef26(void); /* 0xfef26 */
void guest_fn_d663c(ef_addr32 table); /* 0xd663c EAX */
void guest_fn_c37ec(void); /* 0xc37ec */
void guest_fn_c3770(void); /* 0xc3770 */
void guest_fn_dc2c0(void); /* 0xdc2c0 */
void guest_fn_e95c0(uint32_t a, uint32_t b, uint32_t c); /* 0xe95c0 EAX/EDX/EBX */
void guest_fn_e977c(uint32_t a, uint32_t b, uint32_t c); /* 0xe977c EAX/EDX/EBX */
void guest_fn_10f071(ef_addr32 file); /* 0x10f071 EAX */
void guest_fn_f3610(int32_t value); /* 0xf3610 EAX */
void guest_fn_f35f0(void); /* 0xf35f0 */
uint32_t guest_fn_bcbc0(uint32_t value); /* 0xbcbc0 EAX -> AX */

/* float_helpers1.c */
int32_t vector_length_3c74(int32_t x, int32_t y, int32_t z);
int32_t current_object_metric_66c4(void);
void scaled_entry_point_7ca8(ef_addr32 out, uint8_t index, uint8_t sub);
uint8_t find_min_metric_index_8184(ef_addr32 context);
double sub_entry_value_81ec(uint8_t index, uint8_t sub);
double report_waypoint_offset_1d844(ef_addr32 context);
uint8_t waypoint_near_1d8e8(ef_addr32 context);
void scaled_rotation_29adc(ef_addr32 record, ef_addr32 out1, ef_addr32 out2);
void unpack_pair_2aa08(ef_addr32 out, ef_addr32 in);
uint8_t record_close_and_active_2dc84(int16_t index);
double scaled_pair_distance_37bc8(ef_addr32 out, int16_t index);
double guest_fn_a4470(double value); /* 0xa4470 ST0 -> ST0 */
double guest_fn_6a760(int32_t a, int32_t b); /* 0x6a760 EAX/EDX -> ST0 */
double guest_fn_b4230(int32_t value); /* 0xb4230 EAX -> ST0 */
double guest_fn_80f4(ef_addr32 context, uint32_t index); /* 0x80f4 EAX/EDX -> ST0 */
double guest_fn_b42f0(double value); /* 0xb42f0 stack -> ST0 */
double guest_fn_b4620(double first, double second); /* 0xb4620 stack -> ST0 */
double guest_fn_b415c(ef_addr32 a, ef_addr32 b); /* 0xb415c EAX/EDX -> ST0 */

/* float_helpers2.c */
uint8_t record_near_unit_2dbf8(int16_t index, uint8_t unit);
double report_point_offset_37c0c(ef_addr32 position, int16_t index);
uint8_t point_near_unit_37c64(int16_t index, uint8_t unit);
uint8_t point_near_player_37cdc(int16_t index);
uint8_t point_not_far_from_unit_37d88(int16_t index, uint8_t unit);
uint8_t point_not_far_from_player_37e00(int16_t index);
double clock_rate_difference_40870(uint32_t a, ef_addr32 b, uint32_t c, double divisor);
double object_double_value_45b60(ef_addr32 object);
int32_t scale_to_angle_45fc4(double value);
uint8_t scale_to_byte_4600c(double value);
void add_float_time_4fdf4(ef_addr32 minutes, ef_addr32 seconds, float amount);
double float_vector_length_53330(float x, float y, float z);
void refresh_frame_tables_54110(void);
void fill_sine_table_5f944(void);
int32_t guest_fn_40774(uint32_t a, uint32_t b); /* 0x40774 EAX/EDX */
void guest_fn_5414c(void); /* 0x5414c */
void guest_fn_54210(void); /* 0x54210 */
void guest_fn_54274(void); /* 0x54274 */
void guest_fn_54328(void); /* 0x54328 */
void guest_fn_5437c(void); /* 0x5437c */
double guest_fn_b45fa(double value); /* 0xb45fa ST0 -> ST0 */

/* float_helpers3.c */
double entry_distance_80f4(ef_addr32 context, uint32_t index);
void flag_sub_entry_side_8394(uint8_t index, uint8_t sub);
int32_t sub_entry_heading_8424(uint8_t index, uint8_t sub);
void toggle_hint_flag_18118(void);
void init_autopilot_context_1d7a4(ef_addr32 context);
double unit_offset_value_6a5ac(ef_addr32 position, int16_t unit);
double unit_distance_6a66c(int16_t first, int16_t second);
void unit_axis_difference_6a6e8(ef_addr32 out, int16_t first, int16_t second);
void update_camera_state_6bd88(void);
double unit_slot_value_84f60(int16_t unit, uint8_t slot);
void sum_slot_weights_86720(void);
void scale_vector_to_ints_88d90(ef_addr32 out, ef_addr32 vec);
ef_addr32 guest_fn_6a1b4(int32_t unit); /* 0x6a1b4 EAX */
double guest_fn_b41bc(ef_addr32 a, ef_addr32 b); /* 0xb41bc EAX/EDX -> ST0 */
void guest_fn_6c2a4(void); /* 0x6c2a4 */
void guest_fn_6bee8(void); /* 0x6bee8 */
void guest_fn_6be28(void); /* 0x6be28 */

/* float_helpers4.c */
void store_scaled_angles_88ddc(ef_addr32 out, double a0, double a1, double a2);
void matrix_vector_88e50(ef_addr32 out, ef_addr32 vec, ef_addr32 m);
void matrix_vector_88f6c(ef_addr32 out, ef_addr32 vec, ef_addr32 m);
int32_t unit_vector_distance_8bf58(int16_t unit, ef_addr32 vec);
void reset_scene_record_92f10(void);
uint8_t object_faces_camera_97534(ef_addr32 object);
void guest_fn_18a68(void); /* 0x18a68 */
void guest_fn_9357c(void); /* 0x9357c */
void guest_fn_d0ebc(ef_addr32 record, uint32_t value); /* 0xd0ebc EAX/EDX */
void guest_fn_d0dbc(ef_addr32 record, uint32_t value, uint32_t scale); /* 0xd0dbc EAX/EDX/EBX */
void guest_fn_d0fb8(ef_addr32 record, uint32_t value, uint32_t flag); /* 0xd0fb8 EAX/EDX/ECX */
void guest_fn_ea864(ef_addr32 vec, uint32_t a, uint32_t b, uint32_t c); /* 0xea864 EAX/EDX/EBX/ECX */
void guest_fn_ea9ec(ef_addr32 a, ef_addr32 b); /* 0xea9ec EAX/EDX */
int32_t guest_fn_ea9b8(ef_addr32 a, ef_addr32 b); /* 0xea9b8 EAX/EDX -> EAX */

/* float_helpers5.c */
void convert_entry_vector_9ffc8(int16_t index, int16_t value);
void set_record_offsets_a5508(ef_addr32 record, int16_t a, int16_t b);
void set_record_scale_a56b0(ef_addr32 record, double scale);
double distance_2d_b415c(ef_addr32 a, ef_addr32 b);
double distance_3d_b41bc(ef_addr32 a, ef_addr32 b);
double word_to_scaled_b4230(int16_t value);
int32_t double_to_scaled_b4250(double value);
double abs_double_b427c(double value);
double sign_double_b42b0(double value);
double wrap_angle_b42f0(double value);
double vector_length_b4364(ef_addr32 vec);
void guest_fn_ea9a0(ef_addr32 out, int32_t a, int32_t b, int32_t c); /* 0xea9a0 EAX/EDX/EBX/ECX */
void guest_fn_a558c(ef_addr32 record); /* 0xa558c EAX */
double guest_fn_b3fd0(double value); /* 0xb3fd0 stack -> ST0 */

/* float_helpers6.c */
void dispatch_entry_ratio_a3824(uint8_t index);
double match_ratio_b0718(uint8_t type, uint32_t arg, ef_addr32 listp);
double match_ratio_b07b4(uint8_t type, uint32_t arg, ef_addr32 listp);
double dot_product_b43dc(ef_addr32 a, ef_addr32 b);
uint8_t normalize_vector_b4424(ef_addr32 out, ef_addr32 in);
void project_vector_b4468(ef_addr32 out, ef_addr32 dir, ef_addr32 vec);
void reject_vector_b449c(ef_addr32 out, ef_addr32 dir, ef_addr32 vec);
double atan2_b4620(double y, double x);
int32_t default_range_cbfa0(ef_addr32 object);
double scale_view_x_ce0b0(float value, float factor);
double scale_view_y_ce0cc(float value, float factor);
void guest_fn_a3804(uint32_t kind); /* 0xa3804 EAX */
void guest_fn_a37c4(uint32_t kind); /* 0xa37c4 EAX */
void guest_fn_a37e4(uint32_t kind); /* 0xa37e4 EAX */
uint32_t guest_fn_cc1ac(uint32_t a, uint32_t b, uint32_t c); /* 0xcc1ac EAX/EDX/EBX -> AX */
uint32_t guest_fn_cc22c(uint32_t a, uint32_t b, uint32_t c); /* 0xcc22c EAX/EDX/EBX -> AX */
double guest_fn_dc1e9(double a, double b); /* 0xdc1e9 ST1/ST0 -> ST0 */

/* leaf_helpers1.c */
void log_alloc_e764(uint32_t a);
uint32_t constant_44_35218(void);
void store_word_39300(void);
double scale_float_52eb0(float value);
void run_mode_one_56c7c(uint32_t a);
uint8_t low_byte_7a290(void);
void register_handler_7a38c(void);
uint8_t return_zero_9d778(void);
void nop_9d77c(void);
void nop_9dddc(void);
void poll_device_a4104(void);
ef_addr32 apply_to_widget_a75b4(ef_addr32 root, ef_addr32 name, uint32_t value);
uint32_t seek_start_a7ab4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b);
uint32_t seek_current_a7ac8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b);
uint32_t seek_end_a7ae0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b);
void build_default_path_abaf0(ef_addr32 buffer);
void fill_bytes_b3fb0(ef_addr32 dst, uint8_t value, uint32_t count);
double square_double_b3fd0(double x);
double cube_double_b3fdc(double x);
ef_addr32 swap_word_c5390(uint32_t u0, uint32_t u1, ef_addr32 ptr);
ef_addr32 swap_word_c539c(uint32_t u0, uint32_t u1, ef_addr32 ptr);
ef_addr32 swap_word_c53a8(uint32_t u0, uint32_t u1, ef_addr32 ptr);
ef_addr32 swap_dword_c53df(uint32_t u0, uint32_t u1, ef_addr32 ptr);
void log_value_d175c(uint32_t value);
uint32_t stack_used_dcb74(void);
void run_shutdown_pair_dd8d0(void);
uint32_t format_to_buffer_e3022(ef_addr32 buffer, ef_addr32 format, ef_addr32 args);
void set_scratch_name_e3ffc(ef_addr32 name);
uint32_t forward_or_clear_ea4a7(uint32_t value);
void report_error_14_ebe79(void);
void call_110123_f9ecd(void);
void reset_with_saved_byte_109070(void);
void run_pair_109440(void);
uint32_t return_minus_one_10ddf7(void);
void run_with_ebx_10e4d3(uint32_t a, uint32_t b);
uint32_t return_zero_1105c3(void);
void discard_three_1105d0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b,
                          uint32_t stack_c);
void discard_two_1105d4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b);
uint32_t dword_shr8_1115f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_ptr);
void set_record_field_1141a0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_record,
                             uint32_t stack_value);
double box_width_11af88(ef_addr32 box);
double box_extent_sum_11af9c(ef_addr32 box);
double box_extent_ratio_11afe4(ef_addr32 box);
double constant_double_11b3c0(void);
double level_below_11b694(ef_addr32 box, double value);
double level_above_11b6a8(ef_addr32 box, double value);
void disable_interrupts_11f2d3(void);
void enable_interrupts_11f2d5(void);
int32_t record_word_16010e(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_id);
uint32_t tail_jump_39180(uint32_t a);
uint32_t tail_jump_56a44(uint32_t a);
uint32_t tail_jump_58094(uint32_t a);
uint32_t tail_jump_9d088(uint32_t a);
uint32_t tail_jump_cc768(uint32_t a);
uint32_t tail_jump_ab954(uint32_t a);
uint32_t tail_jump_ac7c0(uint32_t a);
uint32_t tail_jump_c3f70(uint32_t a);
uint32_t tail_jump_c3f78(uint32_t a);
uint32_t tail_jump_c3f80(uint32_t a);
uint32_t tail_jump_cbc9b(uint32_t a);
uint32_t tail_jump_11c0a4(uint32_t a);
uint32_t tail_jump_e5bfe(uint32_t a);
uint32_t tail_jump_101e70(uint32_t a);
uint32_t tail_jump_101e78(uint32_t a);
uint32_t tail_jump_101f70(uint32_t a);
uint32_t tail_jump_102020(uint32_t a);
uint32_t tail_jump_106be0(uint32_t a);
uint32_t tail_jump_115984(uint32_t a);
uint32_t tail_jump_11d24e(uint32_t a);
void tail_jump_c9ec4(void);
uint32_t tail_jump_9c23d(void);
uint32_t guest_tail_109e6c(uint32_t a); /* 0x109e6c EAX */
uint32_t guest_tail_110dcc(uint32_t a); /* 0x110dcc EAX */
uint32_t guest_tail_110dd9(uint32_t a); /* 0x110dd9 EAX */
uint32_t guest_tail_1111e1(uint32_t a); /* 0x1111e1 EAX */
uint32_t guest_tail_111359(uint32_t a); /* 0x111359 EAX */
uint32_t guest_tail_120485(uint32_t a); /* 0x120485 EAX */
uint32_t guest_tail_7a290(uint32_t a); /* 0x7a290 EAX */
uint32_t guest_tail_8fbc8(uint32_t a); /* 0x8fbc8 EAX */
uint32_t guest_tail_9c224(uint32_t a); /* 0x9c224 EAX */
uint32_t guest_tail_a3f30(uint32_t a); /* 0xa3f30 EAX */
uint32_t guest_tail_abec4(uint32_t a); /* 0xabec4 EAX */
uint32_t guest_tail_c3f70(uint32_t a); /* 0xc3f70 EAX */
uint32_t guest_tail_c3f80(uint32_t a); /* 0xc3f80 EAX */
uint32_t guest_tail_c9fc4(uint32_t a); /* 0xc9fc4 EAX */
uint32_t guest_tail_f8248(uint32_t a); /* 0xf8248 EAX */
uint32_t guest_tail_fa9a0(uint32_t a); /* 0xfa9a0 EAX */
uint32_t guest_tail_fa9d2(uint32_t a); /* 0xfa9d2 EAX */
uint32_t guest_tail_fc354(uint32_t a); /* 0xfc354 EAX */
uint32_t guest_tail_fc795(uint32_t a); /* 0xfc795 EAX */
uint32_t guest_tail_fe541(uint32_t a); /* 0xfe541 EAX */
void guest_tail_dc2c0(void); /* 0xdc2c0 */
void guest_fn_cc6c0_pair(uint32_t a, ef_addr32 format); /* 0xcc6c0 cdecl */
uint32_t guest_fn_9ddfc_w(void); /* 0x9ddfc -> AX */
void guest_fn_56c90_m(uint32_t a, uint32_t mode); /* 0x56c90 EAX/EDX */
uint32_t guest_fn_7a194_r(void); /* 0x7a194 -> EAX */
void guest_fn_e7fa4_h(uint32_t a, uint32_t b); /* 0xe7fa4 EAX/EDX */
void guest_fn_a3fe0_p(void); /* 0xa3fe0 */
void guest_fn_a7588_v(ef_addr32 widget, uint32_t value); /* 0xa7588 EAX/EDX */
uint32_t guest_fn_a7a64_r(uint32_t handle, uint32_t mode, uint32_t offset); /* 0xa7a64 EAX/EDX/EBX -> EAX */
void guest_fn_deb50_f(ef_addr32 dst, uint32_t value, uint32_t count); /* 0xdeb50 EAX/EDX/ECX */
void guest_fn_dd870_s(void); /* 0xdd870 */
void guest_fn_ddae0_s(void); /* 0xddae0 */
uint32_t guest_fn_ed133_f(ef_addr32 buffer, ef_addr32 format, ef_addr32 args, uint32_t callback); /* 0xed133 EAX/EDX/EBX/ECX */
void guest_fn_107310_s(void); /* 0x107310 */
uint32_t guest_tail_fa8a0(uint32_t a); /* 0xfa8a0 EAX */
void guest_fn_10ddf7_s(void); /* 0x10ddf7 */
void guest_fn_110123_s(void); /* 0x110123 */
void guest_fn_e6120_s(uint32_t a); /* 0xe6120 EAX */
void guest_fn_e6144_s(uint32_t a); /* 0xe6144 EAX */
void guest_fn_10944c_s(void); /* 0x10944c */
void guest_fn_118b60_s(void); /* 0x118b60 */
void guest_fn_10e4dd_s(uint32_t a, uint32_t b, uint32_t c); /* 0x10e4dd EAX/EDX/EBX */
ef_addr32 guest_fn_b3f58_id(uint32_t id); /* 0xb3f58 EAX */

/* leaf_helpers2.c */
uint32_t dos_call_2503_324(uint32_t a);
void dos_call_2505_3d0(uint32_t a, uint32_t b);
void dos_call_3503_c24(int16_t a, uint32_t b);
void run_named_widget_a5bc(void);
void run_named_widget_7cc38(void);
void register_callback_84d94(void);
void latch_decoded_key_a29cc(uint8_t key);
void issue_state_request_a32c8(uint8_t value);
void copy_constants_c469b(void);
void restore_video_mode_cdc64(void);
uint8_t refresh_cached_chunk_ea3ec(ef_addr32 descriptor);
int32_t set_default_file_buffer_11159a(ef_addr32 file, ef_addr32 buffer);
uint32_t entry_empty_118520(ef_addr32 record);
void run_table_initialisers_1196a0(void);
void store_derived_pair_11f538(ef_addr32 out, uint32_t value);
void guest_fn_c868_w(ef_addr32 widget); /* 0xc868 EAX */
void guest_fn_c768_w(uint32_t value); /* 0xc768 EAX */
void guest_fn_7bc40_w(ef_addr32 widget); /* 0x7bc40 EAX */
void guest_fn_abf8c_w(uint32_t a, uint32_t b, uint32_t c); /* 0xabf8c EAX/EDX/EBX */
void guest_fn_ab838_w(uint32_t callback); /* 0xab838 EAX */
void guest_fn_a29f8_w(uint32_t key); /* 0xa29f8 EAX */
uint8_t guest_fn_a2948_w(uint32_t key, uint32_t flag); /* 0xa2948 EAX/EDX -> AL */
void guest_fn_cdc38_w(void); /* 0xcdc38 */
uint32_t guest_fn_fe700_w(void); /* 0xfe700 -> AX */
void guest_fn_fe710_w(int32_t mode); /* 0xfe710 EAX */
uint32_t guest_fn_ea20c_w(ef_addr32 descriptor); /* 0xea20c EAX -> EAX */
void guest_fn_1196c8_w(void); /* 0x1196c8 */
void guest_fn_119844_w(void); /* 0x119844 */
void guest_fn_1198ec_w(void); /* 0x1198ec */
void guest_fn_119938_w(void); /* 0x119938 */
void guest_fn_1199c8_w(void); /* 0x1199c8 */
void guest_fn_119a54_w(void); /* 0x119a54 */
void guest_fn_119acc_w(void); /* 0x119acc */
void guest_fn_11c20c_w(void); /* 0x11c20c */
uint32_t guest_fn_11f4d8_w(uint32_t value); /* 0x11f4d8 EAX -> EAX */

/* leaf_helpers3.c */
uint32_t find_pending_key_a60(void);
void reset_scroll_state_1f960(void);
void toggle_named_widget_7bc00(void);
uint8_t refresh_view_844b0(void);
void recompute_scene_extents_9202c(void);
void update_sound_chain_9ab30(void);
void flush_pending_bytes_a3f30(void);
void refresh_named_widget_abce8(void);
void fill_pattern_deb50(ef_addr32 dst, uint32_t pattern, uint32_t count);
void store_args_and_shutdown_e474c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a,
                                   uint32_t stack_b);
void guest_fn_22e98_w(uint32_t a, uint32_t b); /* 0x22e98 EAX/EDX */
uint8_t guest_fn_843b0_w(void); /* 0x843b0 -> AL */
void guest_fn_84370_w(void); /* 0x84370 */
uint32_t guest_fn_d13ac_w(ef_addr32 a); /* 0xd13ac EAX -> AX */
uint32_t guest_fn_d13dc_w(ef_addr32 a, ef_addr32 b); /* 0xd13dc EAX/EDX -> AX */
uint32_t guest_fn_9a544_w(void); /* 0x9a544 -> EAX */
void guest_fn_9ab6c_w(uint32_t obj); /* 0x9ab6c EAX */
void guest_fn_9ac84_w(uint32_t obj); /* 0x9ac84 EAX */
void guest_fn_9ae44_w(uint32_t obj); /* 0x9ae44 EAX */
void guest_fn_9b04c_w(uint32_t obj); /* 0x9b04c EAX */
void guest_fn_ca5c_w(uint32_t obj); /* 0xca5c EAX */
void guest_fn_9b2a4_w(uint32_t obj); /* 0x9b2a4 EAX */
void guest_fn_bc421_w(uint32_t obj); /* 0xbc421 EAX */
void guest_fn_e6990_w(void); /* 0xe6990 */
void guest_fn_109440_w(void); /* 0x109440 */
void guest_fn_1098f0_w(void); /* 0x1098f0 */
void guest_fn_109a80_w(void); /* 0x109a80 */
void guest_fn_109b20_w(void); /* 0x109b20 */
void guest_fn_d6090_w(void); /* 0xd6090 */
void guest_fn_e4340_w(void); /* 0xe4340 */

/* leaf_helpers4.c */
void reset_menu_page_5f634(void);
uint8_t apply_step_6e390(uint8_t a, uint8_t b);
void select_page_handler_81110(void);
void advance_page_counter_83b50(void);
uint8_t refresh_panel_84544(void);
void play_queued_messages_84c84(void);
void request_status_string_a3280(uint16_t index);
void request_entry_details_a334c(uint16_t index);
void run_pending_callbacks_a5abc(void);
void step_list_cursor_a9490(ef_addr32 list, int16_t n);
void guest_fn_5e658_w(void); /* 0x5e658 */
void guest_fn_5e4c4_w(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0x5e4c4 EAX/EDX/EBX/ECX/stack */
void guest_fn_5ea64_w(void); /* 0x5ea64 */
void guest_fn_86920_w(void); /* 0x86920 */
uint8_t guest_fn_6e320_w(uint32_t a, uint32_t b); /* 0x6e320 EAX/EDX -> AL */
void guest_fn_6e3e0_w(double first, double second); /* 0x6e3e0 stack */
uint32_t guest_fn_d6e58_w(void); /* 0xd6e58 -> EAX */
void guest_fn_82e40_w(void); /* 0x82e40 */
void guest_fn_7fe8c_w(void); /* 0x7fe8c */
void guest_fn_810d0_w(uint32_t a); /* 0x810d0 EAX */
void guest_fn_810f4_w(uint32_t a); /* 0x810f4 EAX */
void guest_fn_dc334_w(void); /* 0xdc334 */
uint8_t guest_fn_834e8_w(int32_t a, int32_t b); /* 0x834e8 EAX/EDX -> AL */
void guest_fn_dc832_w(uint32_t a, uint32_t b, uint32_t c); /* 0xdc832 EAX/EDX/EBX */
void guest_fn_ac7c0_w(void); /* 0xac7c0 */
void guest_fn_ac0e0_w(void); /* 0xac0e0 */
void guest_fn_ac408_w(uint32_t text); /* 0xac408 EAX */
uint32_t guest_fn_ac5f8_w(uint32_t a); /* 0xac5f8 EAX -> EAX */
void guest_fn_e9a24_w(uint32_t a, uint32_t b); /* 0xe9a24 cdecl */

/* leaf_helpers5.c */
uint8_t probe_data_file_a2948(uint8_t key);
void relabel_named_widget_abc34(void);
void draw_named_widget_abc98(void);
void init_camera_table_d7b6c(void);
uint32_t open_stream_formats_d9c30(void);
void read_stream_header_d9dc0(void);
void draw_active_marker_e5088(void);
uint8_t guest_fn_e93dc_r(uint32_t a, uint32_t b); /* 0xe93dc EAX/EDX -> AL */
uint32_t guest_fn_106be8_w(uint32_t a, uint32_t b, uint32_t c); /* 0x106be8 EAX/EDX/EBX -> EAX */
uint8_t guest_fn_107004_w(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x107004 EAX/EDX/EBX/ECX -> AL */
void guest_fn_10703c_w(uint32_t a); /* 0x10703c EAX */
uint8_t guest_fn_106e2c_w(uint32_t a, uint32_t b, uint32_t c); /* 0x106e2c EAX/EDX/EBX -> AL */
uint8_t guest_fn_107070_w(uint32_t a, uint32_t b); /* 0x107070 EAX/EDX -> AL */
uint8_t guest_fn_106d34_w(uint32_t a, uint32_t b); /* 0x106d34 EAX/EDX -> AL */
uint32_t guest_fn_106bfc_w(uint32_t a); /* 0x106bfc EAX -> EAX */
void guest_fn_107060_w(uint32_t a); /* 0x107060 EAX */
void guest_fn_e4c9c_w(ef_addr32 unit); /* 0xe4c9c EAX */
void guest_fn_c91c8_w(uint32_t color, uint32_t x, uint32_t y, uint32_t x2); /* 0xc91c8 EAX/EDX/EBX/ECX */

/* leaf_helpers6.c */
void load_settings_39254(void);
void bind_page_widgets_42f7c(void);
void redraw_panel_5cff0(void);
void apply_options_82cec(void);
void run_animation_loop_a8958(ef_addr32 record);
void restore_page_cd554(void);
uint32_t guest_fn_9ddfc_n(uint32_t name); /* 0x9ddfc EAX -> AX */
void guest_fn_68a28_w(void); /* 0x68a28 */
void guest_fn_39204_w(void); /* 0x39204 */
void guest_fn_ce328_w(int32_t a, int32_t b, int32_t c, int32_t d); /* 0xce328 EAX/EDX/EBX/ECX */
void guest_fn_5d48c_w(int32_t a, int32_t b); /* 0x5d48c EAX/EDX */
void guest_fn_5d080_w(uint32_t a); /* 0x5d080 EAX */
void guest_fn_9cfdc_w(uint32_t a); /* 0x9cfdc EAX */
void guest_fn_30214_w(void); /* 0x30214 */
void guest_fn_2ff44_w(void); /* 0x2ff44 */
void guest_fn_f80_w(void); /* 0xf80 */
void guest_fn_f2660_w(uint32_t a); /* 0xf2660 EAX */
void guest_fn_a82f0_w(ef_addr32 a); /* 0xa82f0 EAX */
void guest_fn_a8718_w(ef_addr32 a); /* 0xa8718 EAX */
uint32_t guest_fn_a7be8_w(uint32_t a); /* 0xa7be8 EAX -> EAX */
void guest_fn_de648_w(uint32_t a); /* 0xde648 EAX */
void guest_fn_6b8_w(uint32_t a); /* 0x6b8 EAX */
void guest_fn_7b4_w(uint32_t a); /* 0x7b4 EAX */
void guest_fn_cd414_w(uint32_t a); /* 0xcd414 EAX */

/* game_helpers53.c */
void try_fire_selected_weapon_1abbc(void);
void refresh_weapon_state_8cb60(void);
void init_scene_angles_94000(void);
void count_ground_kill_9a8b4(ef_addr32 record, uint32_t kind);
void count_air_kill_9a998(ef_addr32 record);
void guest_fn_17754_w(void); /* 0x17754 */
void guest_fn_8cae8_w(void); /* 0x8cae8 */
void guest_fn_8ca24_w(int32_t a); /* 0x8ca24 EDX */
void guest_fn_86720_w(void); /* 0x86720 */
void guest_fn_d9ba0_w(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f); /* 0xd9ba0 EAX/EDX/EBX/ECX/stack */
ef_addr32 guest_fn_c1b8c_w(int32_t a, ef_addr32 b); /* 0xc1b8c EAX/EDX -> EAX */
uint32_t guest_fn_cc584_w(uint32_t a); /* 0xcc584 EAX -> AX */
uint32_t guest_fn_cc5fc_w(uint32_t a); /* 0xcc5fc EAX -> AX */
uint32_t guest_fn_cc628_w(uint32_t a); /* 0xcc628 EAX -> AX */
uint32_t guest_fn_cc654_w(uint32_t a); /* 0xcc654 EAX -> AX */
uint32_t guest_fn_cc67c_w(uint32_t a); /* 0xcc67c EAX -> AX */
uint32_t guest_fn_cc694_w(uint32_t a); /* 0xcc694 EAX -> AX */
uint32_t guest_fn_cc6ac_w(uint32_t a); /* 0xcc6ac EAX -> AX */
uint32_t guest_fn_41e24_w(uint32_t a); /* 0x41e24 EAX -> EAX */
uint32_t guest_fn_41f00_w(uint32_t a); /* 0x41f00 EAX -> EAX */
uint32_t guest_fn_41ea0_w(uint32_t a); /* 0x41ea0 EAX -> EAX */
uint32_t guest_fn_41df4_w(uint32_t a); /* 0x41df4 EAX -> EAX */
uint32_t guest_fn_41ec8_w(uint32_t a); /* 0x41ec8 EAX -> EAX */
uint32_t guest_fn_41e84_w(uint32_t a); /* 0x41e84 EAX -> EAX */

/* game_helpers54.c */
void update_panel_pages_42e8c(void);
uint8_t box_outside_bounds_832b8(int32_t a, int32_t b, int32_t c);
void reset_scene_angle_state_919d4(void);
void init_scene_angles_93e58(void);
void apply_orientation_input_91f30(void);
void guest_fn_a4c84_w(void); /* 0xa4c84 */
void guest_fn_cbc9b_w(uint32_t a); /* 0xcbc9b EAX */
void guest_fn_ac7a8_w(uint32_t a); /* 0xac7a8 EAX */
void guest_fn_421c0_w(void); /* 0x421c0 */
void guest_fn_aca88_w(void); /* 0xaca88 */
void guest_fn_d290_w(void); /* 0xd290 */
void guest_fn_ba5c_w(void); /* 0xba5c */
void guest_fn_a6018_w(uint32_t a); /* 0xa6018 EAX */
void guest_fn_aca94_w(void); /* 0xaca94 */
void guest_fn_a4d90_w(uint32_t a); /* 0xa4d90 EAX */
uint32_t guest_fn_7b7d8_w(void); /* 0x7b7d8 -> EAX */
uint64_t guest_fn_d9b30_w(void); /* 0xd9b30 -> EDX:EAX */

/* game_helpers55.c */
void step_activity_slot_8654(uint8_t group, uint8_t slot);

/* game_helpers55.c */
void step_activity_slot_8654(uint8_t group, uint8_t slot);

/* game_helpers55.c */
void step_activity_slot_8654(uint8_t group, uint8_t slot);
void accumulate_ticks_53c10(void);
void reset_motion_limits_70698(void);
void start_page_7feb0(void);
void leave_scroll_mode_c39ac(void);
void enter_scroll_mode_c39f0(uint32_t value);
void guest_fn_53350_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, float s1, uint32_t s2); /* 0x53350 stack */
void guest_fn_6e8d8_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double v); /* 0x6e8d8 stack */
void guest_fn_6eaf4_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double v); /* 0x6eaf4 stack */
void guest_fn_80120_w(void); /* 0x80120 */

/* game_helpers56.c */
void accumulate_ticks_53c10(void);
void reset_motion_limits_70698(void);
void start_page_7feb0(void);
void leave_scroll_mode_c39ac(void);
void enter_scroll_mode_c39f0(uint32_t value);
void guest_fn_53350_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, float s1, uint32_t s2); /* 0x53350 stack */
void guest_fn_6e8d8_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double v); /* 0x6e8d8 stack */
void guest_fn_6eaf4_w(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double v); /* 0x6eaf4 stack */
void guest_fn_80120_w(void); /* 0x80120 */

/* game_helpers57.c */
int32_t open_slot_a7900(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg);
int32_t open_slot_a7948(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg);
void release_pending_handle_eae10(void);
void apply_selection_f1d54(void);
uint32_t guest_fn_bc36a_w(uint32_t a, uint32_t b); /* 0xbc36a EAX/EDX */

/* game_helpers58.c */
void reset_table_record_112b1c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_rec);

/* float_helpers7.c */
double divide_by_scaled_word_f8314(int16_t n, uint32_t u1, uint32_t u2, uint32_t u3, double value);
double int_distance_2d_f83d0(int32_t x1, int32_t x2, int32_t y1, int32_t y2);

/* float_helpers8.c */
double cos_b45a2(double angle);
double sin_b45ac(double angle);
uint8_t point_in_oriented_box_7d20(ef_addr32 point, ef_addr32 box);
ef_addr32 mark_entry_at_point_82b8(int32_t x, int32_t y);

/* float_helpers9.c */
void update_match_ratio_c964(void);
double pick_best_weapon_16bac(int32_t record_index, ef_addr32 kind_out, ef_addr32 id_out);

/* game_helpers59.c */
void store_descriptor_entry_57480(ef_addr32 src);
void refresh_selection_frame_8bb50(void);
void aim_scene_at_station_93060(void);
void setup_scene_camera_95ae0(void);
void guest_fn_cd20c_w(uint32_t a); /* 0xcd20c EAX */
void guest_fn_8dd48_w(void); /* 0x8dd48 */
void guest_fn_90828_w(void); /* 0x90828 */
void guest_fn_7eccc_w(void); /* 0x7eccc */
void guest_fn_7e8c0_w(uint32_t a); /* 0x7e8c0 EAX */
void guest_fn_7e7e0_w(uint32_t a, uint32_t b); /* 0x7e7e0 EAX/EDX */
void guest_fn_92ffc_w(void); /* 0x92ffc */
void guest_fn_957c8_w(uint32_t a); /* 0x957c8 EAX */

/* game_helpers60.c */
uint32_t scan_code_to_function_key_beb20(uint8_t code);

/* game_helpers60.c */
uint32_t scan_code_to_function_key_beb20(uint8_t code);

/* game_helpers61.c */
void reset_working_state_e76c8(void);
void read_plain_entry_f2e74(void);
void read_entry_f2fbc(void);

/* game_helpers62.c */
uint32_t alias_code_address_fd404(uint32_t address);

/* game_helpers62.c */
uint32_t alias_code_address_fd404(uint32_t address);

/* game_helpers63.c */
void apply_tactical_code_509bc(ef_addr32 record);
void check_unit_leaves_map_3dbf0(void);
void replay_plane_grid_7a9bc(void);
void draw_player_coordinates_5a998(void);
void guest_fn_50b68_w(uint32_t a, uint32_t b); /* 0x50b68 EAX/EDX */
void guest_fn_5086c_w(void); /* 0x5086c */
void guest_fn_a1b54_w(uint32_t a, uint32_t b, uint32_t c); /* 0xa1b54 EAX/EDX/EBX */
void guest_fn_a1d00_w(int32_t a); /* 0xa1d00 EAX */
void guest_fn_5c7dc_w(uint32_t a); /* 0x5c7dc EAX */
void guest_fn_59adc_w(void); /* 0x59adc */

/* game_helpers64.c */
void request_status_for_a32e8(uint16_t key);
void redraw_wide_span_d3c50(void);
int32_t guest_fn_a312c_w(uint32_t a); /* 0xa312c EAX */
void guest_fn_d3900_w(void); /* 0xd3900 */

/* game_helpers65.c */
uint8_t latch_menu_entry_df65c(void);
uint8_t reload_halfword_table_e3ad0(void);
void guest_fn_e393c_w(void); /* 0xe393c */
int32_t guest_fn_e3908_w(int32_t a); /* 0xe3908 EAX */
void guest_fn_c4654_w(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xc4654 EAX/EDX/EBX/ECX */

/* game_helpers66.c */
uint8_t weapon_flags_any_69a40(int16_t index);
void clear_named_references_8ef38(ef_addr32 object);
void switch_player_mode_17f10(void);
void guest_fn_7eec4_w(void); /* 0x7eec4 */
int32_t guest_fn_85d00_w(int32_t a); /* 0x85d00 EAX -> AX */

/* game_helpers67.c */
void snapshot_unit_state_7222c(int16_t unit);
void select_cursor_mode_6697c(void);
void guest_fn_73d50_w(int32_t a); /* 0x73d50 EAX */

/* game_helpers68.c */
uint8_t menu_step_back_df4f4(void);
void stream_saved_tables_e393c(void);
void guest_fn_e3170_w(uint32_t addr, uint32_t size); /* 0xe3170 EAX/EDX */
void guest_fn_e330c_w(uint32_t flag, uint32_t addr, uint32_t size); /* 0xe330c EAX/EDX/EBX */

/* matrix_fixed.c */
void matrix_mul_transposed_fceb4(ef_addr32 a, ef_addr32 b, ef_addr32 out);
void matrix_mul_fcffb(ef_addr32 a, ef_addr32 b, ef_addr32 out);

/* game_helpers69.c */
void update_trigger_hold_17754(void);
void simulation_frame_1cde8(void);
void guest_fn_86110_x(void); /* 0x86110 */
uint32_t guest_fn_4904c_x(void); /* 0x4904c -> AL */
void guest_fn_1c16c_x(void); /* 0x1c16c */
void guest_fn_1a3cc_x(void); /* 0x1a3cc */
void guest_fn_1a47c_x(void); /* 0x1a47c */
void guest_fn_1c3d0_x(void); /* 0x1c3d0 */
void guest_fn_1c780_x(void); /* 0x1c780 */
void guest_fn_1abbc_x(void); /* 0x1abbc */
void guest_fn_53270_x(void); /* 0x53270 */
void guest_fn_5331c_x(void); /* 0x5331c */
void guest_fn_1cfd8_x(void); /* 0x1cfd8 */
void guest_fn_791e0_x(void); /* 0x791e0 */
void guest_fn_53ecc_x(void); /* 0x53ecc */
void guest_fn_7957c_x(void); /* 0x7957c */
void guest_fn_1cd90_x(void); /* 0x1cd90 */

/* game_helpers70.c */
uint8_t select_contact_icon_7f9e4(ef_addr32 unit);
void guest_fn_7f850_x(uint32_t a); /* 0x7f850 EAX */

/* game_helpers71.c */
uint8_t scan_code_weight_cbd30(uint8_t code);
uint8_t read_saved_tables_e34e8(void);

/* game_helpers72.c */
uint8_t forward_cursor_vector_845c0(void);
void latch_scene_camera_944f4(void);
void guest_fn_842b4_x(void); /* 0x842b4 */
void guest_fn_84394_x(uint32_t vec, uint32_t dx, uint32_t bx); /* 0x84394 EAX/EDX/EBX */
void guest_fn_c8d30_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s); /* 0xc8d30 EAX/EDX/EBX/ECX + stack */

/* game_helpers73.c */
void step_letter_table_11d44(void);
void refresh_group_entries_12210(int16_t group);
void guest_fn_11e7c_x(int32_t a); /* 0x11e7c EAX */
void guest_fn_12140_x(void); /* 0x12140 */
void guest_fn_11d20_x(int32_t a, int32_t b); /* 0x11d20 EAX/EDX */

/* game_helpers74.c */
void update_scene_roll_941e4(void);
void update_chase_distance_9586c(void);
void draw_hud_frame_9bba4(void);
uint32_t guest_fn_d13ac_x(uint32_t a, uint32_t c); /* 0xd13ac EAX/ECX -> AX */
void guest_fn_c4105_x(void); /* 0xc4105 */
void guest_fn_c99ac_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s); /* 0xc99ac EAX/EDX/EBX/ECX + stack */
void guest_fn_c9a98_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s); /* 0xc9a98 EAX/EDX/EBX/ECX + stack */
void guest_fn_c95ac_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s); /* 0xc95ac EAX/EDX/EBX/ECX + stack */

/* game_helpers75.c */
void install_graphics_dispatch_c448b(void);
void merge_pending_flags_f75e0(void);
uint32_t switch_active_object_f1c58(ef_addr32 state);
uint32_t set_channel_mode_10dc7f(int32_t channel, int32_t mode);
void guest_fn_d1f88_x(void); /* 0xd1f88 */
void guest_fn_abb00_x(uint32_t a); /* 0xabb00 EAX */
void guest_fn_a4b08_x(void); /* 0xa4b08 */
void guest_fn_11ce30_x(void); /* 0x11ce30 */
void guest_fn_11d01c_x(void); /* 0x11d01c */
void guest_fn_11cfe4_x(void); /* 0x11cfe4 */
void guest_fn_11d135_x(void); /* 0x11d135 */
void guest_fn_11d24e_x(void); /* 0x11d24e */

/* game_helpers76.c */
void select_active_entry_1f64(void);
void compute_box_padding_a66cc(ef_addr32 box, uint8_t grow);
uint32_t guest_fn_f60_x(uint32_t a); /* 0xf60 EAX -> AL */
void guest_fn_a63c0_x(uint32_t a); /* 0xa63c0 EAX */

/* game_helpers77.c */
void update_mission_page_34f94(void);
void guest_fn_a4138_x(void); /* 0xa4138 */
uint32_t guest_fn_a41dc_x(void); /* 0xa41dc -> AL */
void guest_fn_a42c4_x(void); /* 0xa42c4 */
void guest_fn_34f30_x(void); /* 0x34f30 */
void guest_fn_a3f0c_x(void); /* 0xa3f0c */
uint32_t guest_fn_a46cc_x(uint32_t name); /* 0xa46cc EAX -> EAX */
void guest_fn_33b5c_x(uint32_t dialog); /* 0x33b5c EAX */
void guest_fn_33888_x(uint32_t dialog); /* 0x33888 EAX */
uint32_t guest_fn_a3bdc_x(void); /* 0xa3bdc -> AL */
void guest_fn_34b2c_x(void); /* 0x34b2c */
void guest_fn_34ea4_x(void); /* 0x34ea4 */

/* float_helpers10.c */
double truncate_9d72c(double value);
void store_scaled_angle_difference_6ebbc(void);
double lookup_scaled_cell_7b040(ef_addr32 point);
void direction_vector_88fb4(ef_addr32 out, uint32_t u1, uint32_t u2, uint32_t u3, double a, double b);
void matrix_transpose_mul_vector_b4508(ef_addr32 out, ef_addr32 vec, ef_addr32 mat);
void matrix_mul_vector_b4550(ef_addr32 out, ef_addr32 vec, ef_addr32 mat);
uint32_t guest_fn_7b16c_x(uint32_t a, uint32_t b, uint32_t c); /* 0x7b16c EAX/EDX/EBX -> EAX */

/* float_helpers11.c */
void transform_by_global_matrix_ea038(ef_addr32 out, ef_addr32 vec);
void update_tick_rate_e3120(void);
uint32_t guest_fn_f8248_x(void); /* 0xf8248 -> EAX */

/* leaf_helpers7.c */
void send_byte_request_8_a37c4(uint8_t value);
void send_byte_request_9_a37e4(uint8_t value);
void send_byte_request_10_a3804(uint8_t value);
void send_zero_request_5_a3f0c(void);
uint8_t map_code_9_10_bbae4(uint8_t code);
uint8_t map_code_5_6_bbb34(uint8_t code);
uint8_t map_code_flag_bbbe4(uint8_t code);
uint8_t map_code_class_bbd8c(uint8_t code);
uint8_t scene_record_unset_940e0(void);
uint32_t real_mode_to_linear_9c1bb(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t seg_off);
void apply_stream_value_5ca60(void);
int32_t guest_fn_86f7c_x(uint32_t a, uint32_t b); /* 0x86f7c EAX/EDX -> EAX */
void guest_fn_5c9d4_x(uint32_t a, uint32_t b); /* 0x5c9d4 EAX/EDX */

/* game_helpers78.c */
void enter_mode_1a358(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg);
void leave_mode_1a390(void);
void reset_channels_3dccc(void);
uint32_t open_list_node_bc421(uint32_t key);
void run_pending_handlers_a61b0(void);
void install_runtime_hook_120485(void);
void guest_fn_d6340_x(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a, uint32_t b, uint32_t c); /* 0xd6340 stack */
void guest_fn_dc2c0_x(uint32_t a, uint32_t i); /* 0xdc2c0 EDX */
void guest_fn_dc280_x(uint32_t a, uint32_t i); /* 0xdc280 EDX */
uint32_t guest_fn_bc450_x(uint32_t a, uint32_t b); /* 0xbc450 EAX/EDX */
void guest_fn_a60a8_x(uint32_t a); /* 0xa60a8 EAX */
void guest_fn_120746_x(void); /* 0x120746 */
void guest_fn_120800_x(void); /* 0x120800 */

/* game_helpers79.c */
void merge_pending_flags2_f7518(void);
void guest_fn_f6124_x(void); /* 0xf6124 */

/* game_helpers80.c */
void copy_detail_fields_88cd0(ef_addr32 object, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c);
void forward_table_entry_6e320(uint8_t slot, uint32_t kind);
void resolve_weapon_names_68efc(void);
void apply_position_record_569b4(ef_addr32 rec);
void bind_dialog_values_4f388(ef_addr32 first, ef_addr32 second);
ef_addr32 rebuild_child_dialog_34a98(ef_addr32 text);
int32_t list_directory_names_32de8(ef_addr32 path, ef_addr32 mask);
void guest_fn_6e1c4_x(uint32_t a); /* 0x6e1c4 EAX */
void guest_fn_106b0_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s); /* 0x106b0 EAX/EDX/EBX/ECX + stack */
void guest_fn_10750_x(int32_t a, int32_t b, int32_t c); /* 0x10750 EAX/EDX/EBX */
void guest_fn_a4dd0_x(uint32_t a); /* 0xa4dd0 EAX */
uint32_t guest_fn_decde_x(uint32_t a, uint32_t b, uint32_t c); /* 0xdecde EAX/EDX/EBX */
uint32_t guest_fn_decfb_x(uint32_t a); /* 0xdecfb EAX */
void guest_fn_dedde_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xdedde EAX/EDX/EBX/ECX */

/* game_helpers81.c */
ef_addr32 format_number_commas_8b328(ef_addr32 dest, int32_t width, int32_t value);
uint8_t scan_directory_entry_8b684(int16_t mode, ef_addr32 name, ef_addr32 suffix, ef_addr32 buf);
uint32_t add_far_offset_9bfe3(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t far_ptr, int32_t delta);
int32_t select_lowest_rated_entry_9fdd4(void);
void build_status_line_a9878(ef_addr32 name);
void guest_fn_9fe1c_x(int32_t a); /* 0x9fe1c EAX */
uint32_t guest_fn_a7b70_x(void); /* 0xa7b70 -> EAX */
void guest_fn_cd554_x(void); /* 0xcd554 */
void guest_fn_cd4d4_x(void); /* 0xcd4d4 */

/* game_helpers81.c */
ef_addr32 format_number_commas_8b328(ef_addr32 dest, int32_t width, int32_t value);
uint8_t scan_directory_entry_8b684(int16_t mode, ef_addr32 name, ef_addr32 suffix, ef_addr32 buf);
uint32_t add_far_offset_9bfe3(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t far_ptr, int32_t delta);
int32_t select_lowest_rated_entry_9fdd4(void);
void build_status_line_a9878(ef_addr32 name);

/* game_helpers82.c */
void print_move_record_b2504(ef_addr32 rec, int16_t value, uint32_t handle);
void print_class_record_b2960(ef_addr32 rec, uint32_t handle);
void print_coordinate_record_b2a2c(ef_addr32 rec, uint32_t handle);

/* game_helpers83.c */
void dump_buffer_b3580(ef_addr32 buffer, uint32_t handle, uint32_t title);
void make_temp_name_bc471(ef_addr32 name, uint32_t index);
uint32_t guest_fn_fa89a_x(void); /* 0xfa89a -> EAX */
uint32_t guest_fn_bc465_x(uint32_t nibble); /* 0xbc465 EAX -> AL */

/* game_helpers84.c */
void rotate_vector_by_angles_32414(ef_addr32 angles, ef_addr32 vec, ef_addr32 out);
void clamp_box_to_container_a69d0(ef_addr32 container, ef_addr32 box);

/* game_helpers85.c */
void copy_ids_of_colocated_records_be3f4(ef_addr32 first_list, ef_addr32 second_list);
void transfer_entry_weights_be77c(ef_addr32 object, ef_addr32 list);
uint32_t instantiate_list_records_bd2f4(ef_addr32 list, uint32_t arg_b, uint32_t dest_head, uint32_t dest_tail, uint32_t extra);
uint32_t guest_fn_bcc2c_x(ef_addr32 node); /* 0xbcc2c EAX -> AX */
uint32_t guest_fn_bcf44_x(ef_addr32 node, uint32_t b, uint32_t c); /* 0xbcf44 EAX EDX EBX -> EAX */

/* game_helpers86.c */
uint32_t byte_swap32_c1bd0(uint32_t value);
void mark_matching_records_cb184(uint32_t x, uint32_t y, uint32_t type, uint32_t count, ef_addr32 list);
void clear_indexed_subentry_flags_cb614(ef_addr32 list, ef_addr32 table, uint32_t count);
uint32_t list_has_matching_entry_cb984(ef_addr32 object, ef_addr32 list, uint32_t extra);
uint32_t guest_fn_cc1ac_x(uint32_t a, uint32_t b, uint32_t c); /* 0xcc1ac EAX EDX EBX -> AX */

/* game_helpers87.c */
uint8_t triangle_is_clockwise_d22d0(ef_addr32 triangle);
void write_log_word_swapped_d1860(uint16_t value);
uint8_t nearest_entry_index_d642c(int32_t value, ef_addr32 table, int32_t count);
void midi_reset_channels_d6024(void);
uint8_t dispatch_code_table_a_dcc98(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val);
uint8_t dispatch_code_table_b_dd164(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val);
void guest_fn_cf7c4_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xcf7c4 4 stack, callee cleanup */
void guest_fn_d605c_x(uint32_t a, uint32_t b, uint32_t c); /* 0xd605c EAX EDX EBX */

/* game_helpers88.c */
void fill_bit_pattern_table_c2f80(ef_addr32 table);
int32_t approx_distance3_ce578(int32_t a, int32_t b, int32_t c);
void compute_vertex_bounds_d1fc0(ef_addr32 object);
void mark_records_by_map_cell_cb55c(ef_addr32 list, ef_addr32 map, uint32_t extra);

/* game_helpers89.c */
void fade_palette_in_de580(ef_addr32 palette);
void fade_palette_out_de648(ef_addr32 palette);
void fade_palette_out_slow_de6c4(ef_addr32 palette);
int32_t text_width_e1df0(ef_addr32 text, int16_t len, uint8_t font);
int32_t half_active_entry_count_fab80(ef_addr32 object);

/* game_helpers90.c */
void draw_rect_outline_e83d8(ef_addr32 p, ef_addr32 q);
void draw_box_outline_e853c(ef_addr32 p, ef_addr32 q);

/* runtime_helpers30.c */
uint32_t parse_decimal_1092b0(ef_addr32 cursor);
void copy_identifier_10934c(ef_addr32 dest, ef_addr32 cursor);
void decode_rle_rows_110894(ef_addr32 src, ef_addr32 dst, int16_t stride, int16_t rows);

/* game_helpers91.c */
void fade_palette_in_slow_de5f4(ef_addr32 palette);
void build_path_with_name_e7cec(ef_addr32 part_a, ef_addr32 part_b, ef_addr32 name, ef_addr32 buffer);
void build_path_from_record_ea17c(ef_addr32 buffer, ef_addr32 record, ef_addr32 extra);
void draw_glyph_string_eb008(uint32_t ctx, uint32_t x, uint32_t y, ef_addr32 text);
void guest_fn_eaf80_x(uint32_t ctx, uint32_t x, uint32_t y, uint32_t glyph); /* 0xeaf80 EAX EDX EBX ECX */

/* game_helpers92.c */
void quicksort_range_e67a0(ef_addr32 object, int16_t lo, int16_t hi, int16_t extra);
uint8_t vertex_depth_range_flat_10162c(ef_addr32 object);
uint32_t guest_fn_e6684_x(ef_addr32 obj, int32_t lo, int32_t hi, int32_t extra); /* 0xe6684 EAX EDX EBX ECX -> EAX */

/* game_helpers93.c */
void update_scene_angle_first(ef_addr32 record, uint16_t value);
void update_scene_angle_second(ef_addr32 record, uint16_t value);
void update_scene_angle_third(ef_addr32 record, uint16_t value);

/* game_helpers94.c */
uint32_t reactivate_records_at_cell_c10c4(int16_t x, int16_t y, uint32_t count, uint8_t type, ef_addr32 head_addr, ef_addr32 list);

/* game_helpers95.c */
void bearing_between_positions_dcaa4(ef_addr32 out, ef_addr32 a, ef_addr32 b);

/* runtime_helpers31.c */
void run_pair_106e2c(uint32_t object, uint32_t u1, uint32_t second);
void reset_table_record_and_link_112bbc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void trace_and_mark_113cf4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void clear_table_entry_1113d4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t index);
void guest_fn_106d00_x(uint32_t a); /* 0x106d00 EAX */
void guest_fn_106dc4_x(uint32_t a, uint32_t b); /* 0x106dc4 EAX EDX */

/* runtime_helpers32.c */
void clear_rows_110b70(ef_addr32 dest, int16_t stride, int16_t len, int16_t count);
uint32_t claim_table_slot_111365(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value);
void table_slot_1_to_2_11141f(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t slot);
void table_slot_2_to_1_111473(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t slot);
void set_table_slot_value_1114c7(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t slot, uint32_t value);
uint32_t read_varlen_111f20(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 cursor);
void guest_fn_110f6f_x(void); /* 0x110f6f */

/* runtime_helpers33.c */
uint32_t file_size_115989(uint32_t handle);
void start_value_ramp_1141b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 obj, int32_t target, int32_t time);
void send_channel_volumes_112bd8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player);
void guest_fn_1123a4_x(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0x1123a4 5 stack args, caller cleanup */

/* game_helpers96.c */
void split_path_e7fc0(ef_addr32 path, ef_addr32 drive, ef_addr32 dir, ef_addr32 name, ef_addr32 ext);
void transform_vector_by_matrix_ea9ec(ef_addr32 vec, ef_addr32 matrix);
uint32_t name_hash_ebfd0(ef_addr32 text);

/* game_helpers97.c */
void make_path_e80d0(ef_addr32 path, ef_addr32 drive, ef_addr32 dir, ef_addr32 name, ef_addr32 ext);

/* game_helpers98.c */
uint32_t partition_records_e6684(ef_addr32 base, int16_t lo, int16_t hi, uint32_t descending);
uint32_t guest_fn_cc6c0_x(uint32_t size, ef_addr32 tag); /* 0xcc6c0 cdecl, two stack args */

/* game_helpers99.c */
uint8_t plane_eval_z_ce550(ef_addr32 plane, uint32_t u1, uint32_t u2, int32_t c);
uint32_t plane_eval_hi_ce4dc(ef_addr32 plane, int32_t x, int32_t y, int32_t z);
uint8_t plane_eval_sign_ce498(ef_addr32 plane, int32_t x, int32_t y, int32_t z);
int32_t compare_nocase_10901d(ef_addr32 a, ef_addr32 b, uint32_t n);
void palette_8_to_6bit_11082c(ef_addr32 data);

/* game_helpers100.c */
void compute_point_bounds_d2040(ef_addr32 object);
void fill_current_table_entry_b570c(ef_addr32 object, ef_addr32 source, uint8_t flag);
void blend_by_angle_weight_a2244(ef_addr32 rec);

/* game_helpers101.c */
void blend_by_complement_weight_a2334(ef_addr32 rec);
void read_next_token_9d278(ef_addr32 ctx);

/* game_helpers102.c */
uint32_t count_records_at_cell_bfb2c(int16_t x, int16_t y, ef_addr32 head);
ef_addr32 parse_format_modifiers_1086f4(ef_addr32 fmt, ef_addr32 spec);
uint32_t guest_fn_bcf18_x(ef_addr32 rec); /* 0xbcf18 EAX -> AX */

/* game_helpers103.c */
int32_t approx_distance3_16_b3830(int16_t a, int16_t b, int16_t c);
uint32_t object_has_ready_entry_bcc2c(ef_addr32 obj);

/* game_helpers104.c */
ef_addr32 parse_output_spec_ed3b4(ef_addr32 fmt, ef_addr32 va_ptr, ef_addr32 spec);
ef_addr32 guest_fn_ed4e7_x(ef_addr32 fmt, ef_addr32 spec); /* 0xed4e7 EAX EDX -> EAX */

/* game_helpers105.c */
ef_addr32 parse_output_flags_ed4e7(ef_addr32 fmt, ef_addr32 spec);
uint32_t get_channel_value_112048(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player, uint32_t status, uint32_t controller);

/* game_helpers106.c */
void set_black_palette_d65e8(void);
void forward_word_args_cc7d0(int16_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1);
void forward_word_args_f8600(int16_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);
void guest_fn_cc90c_x(int32_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1); /* 0xcc90c EAX + 2 stack, callee cleanup */
void guest_fn_f9448_x(int32_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3); /* 0xf9448 EAX + 4 stack, callee cleanup */

/* game_helpers107.c */
void copy_screen_lines_110b30(ef_addr32 src, ef_addr32 dst, int16_t stride);
void dispatch_record_event_1e0ec(ef_addr32 rec, int16_t value);
uint32_t dos_time_hundredths_11004b(void);
uint32_t guest_fn_39548_x(ef_addr32 rec, uint32_t code, int32_t value); /* 0x39548 EAX EDX EBX -> AX */
void guest_fn_11f287_x(ef_addr32 in_regs, ef_addr32 out_regs); /* 0x11f287 EAX EDX (DOS int call) */

/* game_helpers108.c */
ef_addr32 find_entry_at_point_cfa24(uint32_t count, ef_addr32 base, int32_t x, int32_t y);
ef_addr32 flatten_record_blocks_cbf20(ef_addr32 out, ef_addr32 list);
uint32_t guest_fn_cf960_x(ef_addr32 entry, int32_t x, int32_t y); /* 0xcf960 EAX EDX EBX -> EAX */
uint32_t guest_fn_cc6c0_z(uint32_t size, ef_addr32 tag, uint32_t n); /* 0xcc6c0 cdecl, three stack args */

/* game_helpers109.c */
uint8_t draw_glyph_dcf28(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val);

/* game_helpers110.c */
void draw_widget_rect_49144(int16_t idx);
void highlight_widget_22e98(int16_t idx);

/* game_helpers111.c */
void drop_target_if_far_3df8(ef_addr32 position, ef_addr32 current, uint32_t limit);
int32_t stream_fgetc_10f107(ef_addr32 stream);

/* game_helpers112.c */
void reactivate_records_at_object_c08d4(uint8_t type, ef_addr32 object, ef_addr32 list);
void refill_object_cell_c09e4(ef_addr32 object, ef_addr32 list);

/* game_helpers113.c */
void recharge_sub_entries_cac4c(ef_addr32 list);

/* game_helpers114.c */
uint8_t count_ready_entries_bcda0(ef_addr32 obj);

/* game_helpers115.c */
void shuffle_words_b0cfc(ef_addr32 array, int16_t n, uint8_t start, int16_t limit);

/* game_helpers116.c */
int32_t approx_distance3_b39ec(int32_t a, int32_t b, int32_t c);
void update_loadout_maxima_a4eec(ef_addr32 rec);

/* game_helpers117.c */
void deactivate_records_at_object_c07a0(ef_addr32 object, ef_addr32 list);

/* runtime_helpers34.c */
void put_two_digits_f9d98(int32_t value, uint32_t pos, ef_addr32 buffer);
ef_addr32 format_time_f9dcb(ef_addr32 tm, ef_addr32 out);

/* runtime_helpers35.c */
void write_log_dword_swapped_d170c(uint32_t value);
uint32_t attach_buffer_117ed4(ef_addr32 obj, ef_addr32 holder, uint16_t size);
void refresh_player_channels_1142cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player, uint32_t value);

/* game_helpers118.c */
int32_t find_free_slot_run_b58a8(ef_addr32 obj, int16_t start, int16_t need);

/* game_helpers119.c */
uint8_t return_three_a4470(void);
void register_status_description_7aa2c(void);
uint32_t remap_address_d5c84(int32_t addr);

/* game_helpers120.c */
void activate_record_114170(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void teardown_record_1115fc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
int32_t compare_remapped_palette_d660c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 palette);
void register_stack_ranges_d6df0(void);
int32_t angle_of_fitted_pair_dc8d8(int32_t x, int32_t y);
void guest_callptr1_x(ef_addr32 fn, ef_addr32 arg); /* indirect cdecl call with one stack argument */

/* game_helpers121.c */
void deactivate_record_1068b8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void seek_record_eb698(ef_addr32 pos, ef_addr32 ctx);
void copy_name_and_flag_a3424(uint16_t index);

/* runtime_helpers36.c */
void release_record_array_113398(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 ctx);
ef_addr32 stream_fgets_11580e(ef_addr32 buf, int32_t n, ef_addr32 stream);

/* game_helpers122.c */
void refresh_weapon_widgets_69b70(void);
uint32_t count_group_entries_faafc(ef_addr32 group);

/* game_helpers123.c */
void log_palette_d1894(ef_addr32 source, int16_t mode);
void update_frame_slot_b5800(ef_addr32 owner, ef_addr32 obj, int16_t need);

/* game_helpers124.c */
uint32_t point_in_view_volume_ceb40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, int32_t margin, int32_t z, int32_t y, int32_t x);
void register_code_ranges_111575(void);

/* game_helpers125.c */
void draw_tactical_rect_1f7d4(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e);
void upload_vga_palette_feec8(ef_addr32 palette);
void send_record_size_message_1052f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void send_pending_count_message_111df0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);

/* runtime_helpers37.c */
int32_t set_serial_rate_117e0c(ef_addr32 obj, int32_t rate);
int32_t stream_fputc_10f43b(uint32_t c, ef_addr32 stream);
void destroy_record_104464(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);

/* runtime_helpers38.c */
int32_t stream_flush_fa8a0(ef_addr32 stream);
uint32_t guest_fn_110393_x(uint32_t handle, ef_addr32 buf, uint32_t count); /* 0x110393 EAX/EDX/EBX, DOS write */


/* game_helpers126.c */
void print_to_handle_b1bb0(uint32_t handle, ef_addr32 fmt, ...);

/* game_helpers127.c */
void dump_group_b2f48(ef_addr32 group, uint32_t handle);
void dump_group_list_b3038(ef_addr32 list_head, uint32_t handle, uint32_t title);
void dump_entry_list_b1c34(ef_addr32 list_head, uint32_t handle, ef_addr32 fmt);
void dump_point_list_b2e28(uint32_t handle, ef_addr32 list_ptr, int16_t value, ef_addr32 fmt);
void guest_fn_b1f80_x(ef_addr32 node, uint32_t handle); /* 0xb1f80 EAX/EDX */
void guest_fn_b2e98_x(ef_addr32 node, uint32_t handle); /* 0xb2e98 EAX/EDX */

/* game_helpers128.c */
void dump_record_b23bc(ef_addr32 rec, int16_t index, uint32_t handle, int16_t fallback);
void classify_entries_b059c(ef_addr32 list_holder, ef_addr32 head_a, ef_addr32 list_b);
uint32_t guest_fn_b04e8_x(ef_addr32 a, ef_addr32 b); /* 0xb04e8 EAX/EDX -> AX (x87 distance test) */

/* game_helpers129.c */
void guest_memmove_109e6f(ef_addr32 dst, ef_addr32 src, uint32_t n);
void append_midi_event_111e40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t status, uint32_t data1, uint32_t data2);
void append_midi_block_111ed4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, ef_addr32 data, uint32_t length);
void direction_angles_dc91c(ef_addr32 out, ef_addr32 a, ef_addr32 b);
int32_t can_reactivate_c117c(ef_addr32 rec, ef_addr32 list_a, ef_addr32 list_b);

/* game_helpers130.c */
void init_sub_records_b4fb4(ef_addr32 obj, ef_addr32 tmpl);
void dump_area_list_b31f0(ef_addr32 list_holder, uint32_t handle);
uint32_t guest_alloc_fmt_cc6c0(uint32_t size, ef_addr32 fmt, ...); /* 0xcc6c0 cdecl, variadic */

/* runtime_helpers39.c */
ef_addr32 parse_tz_rule_10eef0(ef_addr32 text, ef_addr32 out);
ef_addr32 find_chunk_111f84(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 data, int32_t n);
ef_addr32 break_down_time_10ec62(uint32_t days, uint32_t secs, int32_t offset, ef_addr32 tm);

/* runtime_helpers40.c */
void clear_buffer_117aa2(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void destroy_sound_object_105da0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
void reset_controllers_114058(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);
int32_t stream_refill_10f1bf(ef_addr32 stream);
void guest_fn_117a0f_x(ef_addr32 rec, uint32_t flag); /* 0x117a0f cdecl */
void guest_fn_112808_x(ef_addr32 rec); /* 0x112808 cdecl */
uint32_t guest_fn_11d63f_x(void); /* 0x11d63f: console getch with pushback (DOS int 21h) */
int32_t guest_fn_10f0e8_x(uint32_t handle, ef_addr32 buf, uint32_t count); /* 0x10f0e8 EAX/EDX/EBX: DOS read */

/* runtime_helpers41.c */
void dispatch_midi_event_1149e0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t port, uint32_t status, uint32_t data1, int32_t data2);
uint32_t guest_callptr5_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* indirect cdecl call with five stack arguments */
void guest_fn_1123a4_x(uint32_t port, uint32_t status, uint32_t data1, uint32_t data2, uint32_t zero); /* 0x1123a4 cdecl */

/* runtime_helpers42.c */
int32_t stream_vprintf_fa815(ef_addr32 stream, ef_addr32 fmt, ef_addr32 args);
int32_t stream_printf_bc400(ef_addr32 stream, ef_addr32 fmt, ...);
int32_t stdout_printf_cd12e(ef_addr32 fmt, ...);
int32_t open_file_fa485(ef_addr32 name, uint32_t flags, ...);
int32_t file_size_a7b3c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 name);
int32_t guest_fn_fa4a9_x(ef_addr32 name, uint32_t oflag, uint32_t shflag, uint32_t pmode); /* 0xfa4a9 cdecl (open/sopen) */

void apply_palette_d663c(ef_addr32 palette);
void finish_record_range_d710c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);

/* game_helpers131.c */
int32_t formatted_text_width_48c34(uint32_t mode, ef_addr32 fmt, ...);
void draw_formatted_text_3d308(int32_t col, int32_t row, ef_addr32 fmt, ...);
uint8_t read_entry_block_ebb10(uint32_t key, ef_addr32 buffer, ef_addr32 ctx);
uint32_t for_each_entry_beebc(ef_addr32 a, int16_t arg2, ef_addr32 list, ef_addr32 arg4, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4);
ef_addr32 guest_fn_eb520_x(uint32_t key, ef_addr32 ctx); /* 0xeb520 EAX/EDX */
uint32_t guest_fn_bed1c_x(ef_addr32 a, uint32_t arg2, uint32_t index, ef_addr32 list, ef_addr32 arg4, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0xbed1c EAX/EDX/EBX/ECX + 6 stack */

void draw_formatted_text_mode_48bb0(uint32_t color, uint32_t mode, int32_t x, int32_t y, ef_addr32 fmt, ...);
void place_numbered_widgets_7e01c(int16_t lo, int16_t hi);
void update_box_view_97d10(int16_t x1, int16_t x2, int16_t y1, int16_t y2);
void guest_fn_15a14_x(uint32_t color, int16_t x, int16_t y, ef_addr32 text); /* 0x15a14 EAX/EDX/EBX/ECX */
void guest_fn_e5db4_x(uint32_t color, int16_t x, int16_t y, ef_addr32 text); /* 0xe5db4 EAX/EDX/EBX/ECX */
void guest_fn_e605c_x(uint32_t color, int16_t x, int16_t y, ef_addr32 text); /* 0xe605c EAX/EDX/EBX/ECX */
void guest_fn_dc71b_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e); /* 0xdc71b stack */

void start_record_11402c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec);

void copy_rows_109dd0(ef_addr32 dst, ef_addr32 src, uint32_t len, int32_t count);
void copy_rows_at_109e40(uint32_t x, uint32_t idx, uint32_t len, int32_t count);

/* game_helpers132.c */
void set_resource_addresses_d1f88(void);
void reset_name_record_40e20(void);
int32_t guest_sprintf_a4342(ef_addr32 buf, ef_addr32 fmt, ...);
void note_value_d5fc8(uint32_t value);

/* trace_wrappers1.c */
uint32_t trace_call_101ddc(void);
uint32_t trace_call_101e14(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_101e80(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_101f08(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_101f88(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t trace_call_102144(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_102198(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_1023d8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_10242c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_1024b4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_102508(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_102590(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_1026fc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_102738(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_1028f4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_102dbc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_102f60(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_102fb4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1);
uint32_t trace_call_10303c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_1030bc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_1031cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_1034b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_103778(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2);
uint32_t trace_call_103b08(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0);
uint32_t trace_call_103c28(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
uint32_t trace_call_1023b0(void);
uint32_t guest_fn_104694_t(void); /* 0x104694 cdecl */
uint32_t guest_fn_1041e4_t(uint32_t a0, uint32_t a1); /* 0x1041e4 cdecl */
uint32_t guest_fn_111118_t(uint32_t a0); /* 0x111118 cdecl */
uint32_t guest_fn_1111b1_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x1111b1 cdecl */
uint32_t guest_fn_11124c_t(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3); /* 0x11124c cdecl */
uint32_t guest_fn_111365_t(uint32_t a0); /* 0x111365 cdecl */
uint32_t guest_fn_1113ac_t(uint32_t a0, uint32_t a1); /* 0x1113ac cdecl */
uint32_t guest_fn_104504_t(uint32_t a0); /* 0x104504 cdecl */
uint32_t guest_fn_104200_t(uint32_t a0, uint32_t a1); /* 0x104200 cdecl */
uint32_t guest_fn_106514_t(uint32_t a0); /* 0x106514 cdecl */
uint32_t guest_fn_106464_t(uint32_t a0, uint32_t a1); /* 0x106464 cdecl */
uint32_t guest_fn_1065c8_t(uint32_t a0); /* 0x1065c8 cdecl */
uint32_t guest_fn_106718_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x106718 cdecl */
uint32_t guest_fn_106740_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x106740 cdecl */
uint32_t guest_fn_106708_t(uint32_t a0); /* 0x106708 cdecl */
uint32_t guest_fn_106938_t(uint32_t a0, uint32_t a1); /* 0x106938 cdecl */
uint32_t guest_fn_113af0_t(uint32_t a0); /* 0x113af0 cdecl */
uint32_t guest_fn_113a40_t(uint32_t a0, uint32_t a1); /* 0x113a40 cdecl */
uint32_t guest_fn_113c7c_t(uint32_t a0); /* 0x113c7c cdecl */
uint32_t guest_fn_113d10_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x113d10 cdecl */
uint32_t guest_fn_1141b0_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x1141b0 cdecl */
uint32_t guest_fn_113b78_t(uint32_t a0); /* 0x113b78 cdecl */
uint32_t guest_fn_11451c_t(uint32_t a0, uint32_t a1, uint32_t a2); /* 0x11451c cdecl */
uint32_t guest_fn_1146ec_t(uint32_t a0); /* 0x1146ec cdecl */
uint32_t guest_fn_1149e0_t(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); /* 0x1149e0 cdecl */
uint32_t guest_fn_1113fb_t(void); /* 0x1113fb cdecl */

/* hotspot_draw.c */
void draw_hotspots_447ec(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_4496c(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_44c68(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_44de8(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_450f4(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_45274(ef_addr32 head, ef_addr32 holder);
void draw_hotspots_453f4(ef_addr32 head, ef_addr32 holder);
void guest_fn_440f4_x(uint32_t a, int32_t x, int32_t y, uint32_t w, uint32_t s0, uint32_t s1); /* 0x440f4 EAX/EDX/EBX/ECX + 2 stack */

/* box_rows.c */
void draw_box_rows_99ee4(void);
void draw_box_rows_99fe0(void);
void draw_box_rows_9a0dc(void);
void draw_box_rows_9a1d8(void);

void draw_extent_markers_43834(ef_addr32 head, ef_addr32 holder);
void draw_extent_markers_439a4(ef_addr32 head, ef_addr32 holder);
void draw_extent_markers_43c84(ef_addr32 head, ef_addr32 holder);

/* colour_tables.c */
void fill_colour_record_ba9c4(ef_addr32 rec, uint8_t key);
void fill_colour_record_baa4c(ef_addr32 rec, uint8_t key);
void fill_colour_record_baad4(ef_addr32 rec, uint8_t key);

/* colour_tables2.c */
void fill_colour_record_ba2c0(ef_addr32 rec, uint8_t key);
void fill_colour_record_ba460(ef_addr32 rec, uint8_t key);
void fill_colour_record_ba580(ef_addr32 rec, uint8_t key);
void fill_colour_record_ba6a0(ef_addr32 rec, uint8_t key);
void fill_colour_record_ba804(ef_addr32 rec, uint8_t key);
void fill_colour_record_ba8dc(ef_addr32 rec, uint8_t key);
void fill_colour_record_bab5c(ef_addr32 rec, uint8_t key);
void fill_colour_record_babd0(ef_addr32 rec, uint8_t key);
void fill_colour_record_bac90(ef_addr32 rec, uint8_t key);
void fill_colour_record_bad70(ef_addr32 rec, uint8_t key);
void fill_colour_record_bae78(ef_addr32 rec, uint8_t key);
void fill_colour_record_baf48(ef_addr32 rec, uint8_t key);
void fill_colour_record_bafe4(ef_addr32 rec, uint8_t key);
void fill_colour_record_bb0ac(ef_addr32 rec, uint8_t key);
void fill_colour_record_bb1ac(ef_addr32 rec, uint8_t key);

/* code_maps1.c */
uint8_t map_code_3aa8(uint32_t code);
uint8_t map_code_3b1c(uint32_t code);
uint32_t map_code_74448(uint32_t code);
uint8_t map_code_ba0e4(uint32_t code);

/* small_wrappers1.c */
void forward_trace_call_d605c(uint32_t a, uint32_t b, uint32_t c);

/* small_wrappers2.c */
void close_log_handle_a1cb4(void);
uint32_t file_size_by_name_105024(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 name);
int32_t lookup_name_value_a1a40(ef_addr32 name);

/* small_wrappers3.c */
uint32_t position_out_of_range_aa2f8(ef_addr32 rec);
void select_table_entry_10922c(ef_addr32 name);

/* small_wrappers4.c */
void release_matching_slots_11288c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t id);
void redraw_message_line_77c34(void);
void copy_rows_to_screen_eb0b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, uint32_t src_stride, ef_addr32 src);



/* small_wrappers5.c */
void put_screen_rows_eb050(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, ef_addr32 buf);
void get_screen_rows_eb1f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, ef_addr32 buf);

/* script_helpers1.c */
void run_text_script_1093d8(ef_addr32 text, uint32_t table, uint32_t count);
uint32_t try_handlers_9cbc4(uint32_t a, uint32_t b);
void guest_callptr_eax_x(ef_addr32 fn, ef_addr32 arg); /* indirect call with the argument in EAX */
uint32_t guest_fn_eba64_x(ef_addr32 a, ef_addr32 b, ef_addr32 c); /* 0xeba64 EAX/EDX/EBX */
uint32_t guest_fn_b3c04_x(uint32_t a, uint32_t b); /* 0xb3c04 EAX/EDX, returns EAX */

/* fill_rects1.c */
void fill_rect_ceccc(uint32_t colour, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

/* key_helpers1.c */
int32_t translate_key_1101e8(uint32_t code);



/* table_switches1.c */
void notify_record_action_8c988(ef_addr32 rec);
uint32_t find_node_matching_cc51c(uint32_t id, uint32_t list, uint32_t kind);

/* dos_slots1.c */
void release_hook_slot_7b4(uint32_t idx);
void guest_fn_3f8_x(uint32_t a, uint32_t unused_edx, uint32_t b, uint32_t c); /* 0x3f8 EAX/EDX/EBX/ECX, EDX ignored */

uint32_t wait_ring_data_106dc4(ef_addr32 rec, uint32_t timeout);

/* colour_tables3.c */
void fill_colour_record_ba77c(ef_addr32 rec, uint32_t key);

/* draw_loops1.c */
void draw_record_entries_461e8(uint32_t x, uint32_t index, uint32_t y, uint32_t c, uint32_t s0, uint32_t s1);

/* unit_events1.c */
void unit_event_88664(ef_addr32 unit, uint32_t arg);
void unit_event_886d0(ef_addr32 unit, uint32_t arg);
void guest_fn_87db8_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5); /* 0x87db8 EAX/EDX/EBX/ECX + 6 stack, callee cleanup */

/* init_tables1.c */
void init_record_pool_80d8c(ef_addr32 text);

void select_handlers_9cfdc(ef_addr32 name);

/* fill_rects2.c */
void fill_rect_clipped_c91c8(uint32_t colour, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void set_param_block_dd9e4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);
void guest_fn_dd870_x(void); /* 0xdd870 no arguments */
void guest_fn_dd8c0_x(void); /* 0xdd8c0 no arguments */

/* page_rotate1.c */
void rotate_page_entries_e8368(uint32_t page, uint32_t p0, uint32_t p1, uint32_t p2);

void rebuild_remap_table_e7e30(uint32_t a, uint32_t b, uint32_t c);

/* list_ops1.c */
void remove_entry_and_children_a4a94(uint32_t id);
void draw_record_text_f3aa8(uint32_t index);
void guest_fn_f3904_x(int32_t a, int32_t b, int32_t c, int32_t d); /* 0xf3904 EAX/EDX/EBX/ECX */

void open_named_file_287bc(void);

void step_page_slot_a0a2c(uint32_t index);
void guest_callptr4s2_x(ef_addr32 fn, int32_t a, int32_t b, int32_t c, int32_t d, int32_t s0, int32_t s1); /* indirect call: EAX/EDX/EBX/ECX plus two stack arguments, callee cleanup */

void promote_entry_and_children_a4a20(uint32_t id);
void run_text_on_page_9e494(uint32_t page, ef_addr32 text);
void guest_fn_a48ac_x(uint32_t index); /* 0xa48ac EAX */
void guest_fn_9e7bc_x(ef_addr32 text); /* 0x9e7bc EAX, preserves EBX/ECX/ESI/EDI */

/* probe_helpers1.c */
uint32_t probe_service_d9b30(void);
uint32_t run_probe_e9ac0(void);
void guest_callptr2_x(ef_addr32 fn, uint32_t a, uint32_t b); /* indirect cdecl call with two stack arguments, caller cleanup */

uint32_t point_in_entry_boxes_1e214(uint32_t entry, int32_t x, int32_t y, int32_t z);

/* screen_clip1.c */
void init_work_buffers_c3770(void);
uint32_t reset_ui_state_8bd58(uint32_t a, uint32_t b);
void draw_clipped_c92e4(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);
void guest_fn_8b7e8_x(void); /* 0x8b7e8 no arguments */
uint32_t guest_fn_8bad4_x(uint32_t a, uint32_t b); /* 0x8bad4 EAX/EDX, returns EAX */
void guest_fn_eb174_x(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7); /* 0xeb174 cdecl, eight stack arguments */

void draw_page_entries_c8e8(void);
void guest_fn_11560_x(int32_t a, int32_t b, int32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3); /* 0x11560 EAX/EDX/EBX/ECX + 4 stack, callee cleanup */

/* state_reset1.c */
void reset_input_state_a29f8(void);
void reset_page_record_2ce18(uint32_t page);

void draw_object_entries_46168(uint32_t key, uint32_t b, uint32_t c, uint32_t d, uint32_t s0);

/* load_planes1.c */
void interleave_planes_cdc90(ef_addr32 dst, ef_addr32 src);
uint32_t load_text_file_e94ec(ef_addr32 name, uint32_t flag);
uint32_t guest_callptr_eax_ret_x(ef_addr32 fn, uint32_t a); /* indirect call, argument in EAX, result in EAX */
void guest_callptr_eaxedx_x(ef_addr32 fn, uint32_t a, uint32_t b); /* indirect call, arguments in EAX and EDX */

void init_locked_ranges_103d80(void);
void redraw_pending_message_78ac0(void);
uint32_t guest_fn_48d5c_x(ef_addr32 text); /* 0x48d5c cdecl, one stack argument, caller cleanup */

void commit_mode_selection_35b2c(void);

/* tactical_marks1.c */
void draw_diamond_5d48c(uint32_t x, uint32_t y);

void relink_random_objects_8873c(int32_t chance, ef_addr32 link, uint32_t kind, ef_addr32 new_link);

void draw_path_segments_f3610(uint32_t index);
void guest_fn_e7a34_x(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5); /* 0xe7a34 six stack arguments, callee cleanup */

uint32_t next_matching_slot_85f78(uint32_t key);

void select_page_9e3f8(uint32_t page);

void load_palette_blocks_110790(ef_addr32 data);

void set_clip_and_shade_c9238(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t shade);
void guest_fn_c4b95_x(ef_addr32 block); /* 0xc4b95 one stack argument, callee cleanup */

void save_and_draw_block_dd8dc(void);
void guest_fn_cde00_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xcde00 four stack arguments, callee cleanup */

void load_companion_files_dbf3c(ef_addr32 dest, ef_addr32 part_b, ef_addr32 dest2);

void switch_display_mode_d1e50(uint32_t mode);
void guest_fn_cdb20_x(void); /* 0xcdb20 no arguments */

void measure_page_point_a0aa0(uint32_t index);
void guest_callptr2r_x(ef_addr32 fn, int32_t a, int32_t b); /* indirect call: EAX/EDX register arguments */
void guest_fn_e2374_x(ef_addr32 a, ef_addr32 b); /* 0xe2374 EAX/EDX */

void visit_overlapping_nodes_e2564(ef_addr32 obj);

void decode_rle_image_11092c(ef_addr32 src, ef_addr32 dst, uint32_t stride, ef_addr32 end);

void raise_random_counters_74ed8(uint32_t key, uint32_t n);

void draw_linked_pages_2c600(uint32_t page, uint32_t arg);
void guest_fn_47764_x(int32_t a, uint32_t b, int32_t c, uint32_t d, uint32_t s0); /* 0x47764 EAX/EDX/EBX/ECX + 1 stack, callee cleanup */

void draw_indicator_22f28(uint32_t index);
void guest_fn_387e0_x(uint32_t a, uint32_t b); /* 0x387e0 EAX/EDX */
void guest_fn_38be8_x(uint32_t a, int32_t b, int32_t c); /* 0x38be8 EAX/EDX/EBX */

uint32_t draw_trail_5c8e0(ef_addr32 out);
uint32_t guest_fn_15c0ea_x(uint32_t a0, uint32_t a1, uint32_t a2, ef_addr32 out); /* 0x15c0ea four stack arguments, callee cleanup, result in AL */

/* small_wrappers6.c */
void redraw_if_unsaved_dd8c0(void);
void forward_to_1111e5_101f78(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value);
void guest_fn_1111e5_x(uint32_t a); /* 0x1111e5 cdecl, one stack argument, caller cleanup */

uint32_t release_ref_108ffa(ef_addr32 obj);
void expect_byte_109328(ef_addr32 cursor, uint32_t expected);
void forward_to_11b134_11b1b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void run_with_hook_9e094(uint32_t value, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t arg);
void guest_fn_11b134_x(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3); /* 0x11b134 four stack arguments, callee cleanup */
void guest_fn_9e0c4_x(int32_t value); /* 0x9e0c4 EAX */

void print_status_lines_b1bf8(uint32_t handle);
void report_message_b3374(uint32_t value);
void refresh_for_mode_65a50(void);
void guest_fn_65a88_x(void); /* 0x65a88 no arguments */
void guest_fn_65d24_x(void); /* 0x65d24 no arguments */
void guest_fn_65c1c_x(void); /* 0x65c1c no arguments */

/* small_wrappers7.c */
uint32_t random_step_aaec(void);
uint32_t sum_list_9c1e0(void);
void query_pointer_cdfe8(ef_addr32 x_out, ef_addr32 y_out, ef_addr32 buttons_out);
void run_entry_chain_a0060(void);
uint32_t guest_fn_eb2a8_x(void); /* 0xeb2a8 no arguments, result in EAX */
void guest_fn_a0174_x(ef_addr32 entry); /* 0xa0174 EAX */
void guest_fn_a00e8_x(void); /* 0xa00e8 no arguments */

uint32_t random_delay_a9fc(void);
ef_addr32 utoa_cbcc5(uint32_t value, ef_addr32 dst, uint32_t base);

uint32_t owner_has_weight_8557c(uint32_t owner);
void release_block_d5ec0(ef_addr32 block);

void run_state_handler_84678(void);
void draw_pending_message_912b8(ef_addr32 text);
uint32_t read_ring_bytes_106cb0(ef_addr32 rec, ef_addr32 buf, uint32_t count, uint32_t timeout);
void guest_callptr0_x(ef_addr32 fn); /* indirect call, no arguments */

uint32_t compare_variable_a9640(uint32_t index, uint32_t op, int32_t value);
void restore_saved_block_dd870(void);

uint32_t random_code_aa6c(void);

/* copy_rects1.c */
void copy_rows_from_screen_109e08(uint32_t x, uint32_t idx, uint32_t len, int32_t count);
void save_rect_list_e5114(void);
void restore_rect_list_e516c(void);

/* script_helpers2.c */
void run_section_one_f4c60(void);
void run_section_two_f4d0c(void);


void guest_fn_f46cc(void); /* 0xf46cc no arguments */

uint32_t select_slot_count_fac78(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);
uint32_t select_slot_count_fad54(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);

uint32_t select_wide_slot_count_faf0c(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);
uint32_t select_wide_slot_count_faff0(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);

/* page_lookup1.c */
void lookup_entry_95660(ef_addr32 name);
void lookup_entry_957c8(ef_addr32 name);
void guest_fn_955c0_x(int32_t a, int32_t b, int32_t c, int32_t d, int32_t s0); /* 0x955c0 EAX/EDX/EBX/ECX + 1 stack, callee cleanup */
void guest_fn_95704_x(int32_t a, int32_t b, int32_t c, int32_t d, int32_t s0); /* 0x95704 EAX/EDX/EBX/ECX + 1 stack, callee cleanup */

void draw_glyph_e5c30(uint32_t x, uint32_t y, uint32_t glyph);
void draw_glyph_f2320(uint32_t x, uint32_t y, uint32_t glyph);

/* trace_loaders1.c */
uint32_t load_resource_106464(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t name, uint32_t arg);
uint32_t load_resource_113a40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t name, uint32_t arg);
uint32_t guest_fn_105070_x(uint32_t a0, uint32_t a1); /* 0x105070 cdecl, two stack arguments, caller cleanup */
uint32_t guest_fn_105e38_x(uint32_t a0, uint32_t a1); /* 0x105e38 cdecl, two stack arguments, caller cleanup */
uint32_t guest_fn_1133f8_x(uint32_t a0, uint32_t a1); /* 0x1133f8 cdecl, two stack arguments, caller cleanup */

/* text_metrics1.c */
uint32_t formatted_width_48d5c(ef_addr32 fmt, ...);
uint32_t formatted_width_48da0(ef_addr32 fmt, ...);

void transfer_link_485a8(uint32_t a, uint32_t b);
void transfer_link_486a0(uint32_t a, uint32_t b);

void reload_game_data_8b2d8(void);
void guest_fn_97dcc_x(void); /* 0x97dcc no arguments */

void draw_bar_9bca4(uint32_t colour, uint32_t unused, uint32_t x, uint32_t h);





/* search_helpers1.c */
uint32_t find_text_in_buffer_e0b10(ef_addr32 text);
uint32_t cell_changed_3a268(uint32_t index);
uint32_t guest_fn_85bc8_x(ef_addr32 point); /* 0x85bc8 EAX, result in AL */

void place_rotated_vector_3170c(void);
void drain_kind_two_chain_85380(uint32_t owner);

uint32_t lookup_mapped_code_bbdc4(uint32_t kind, uint32_t index, uint32_t row);

ef_addr32 claim_page_slot_a1770(uint32_t page, uint32_t idx);

void fill_record_f8da8(ef_addr32 dst, ef_addr32 src1, ef_addr32 src2, ef_addr32 owner, uint32_t extra);

uint32_t find_open_entry_1065c8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 obj);



void find_free_save_slot_ac57c(void);
void guest_fn_d1c54_x(ef_addr32 name, uint32_t a, uint32_t mode); /* 0xd1c54 EAX/EDX/EBX */

uint32_t scan_entries_a2ef4(void);

/* misc_drafts1.c */
void open_named_file_28744(void);
void queue_request_36100(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1);
void draw_mode_dispatch_a8660(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t index);
void guest_fn_35de0_x(uint32_t s0, uint32_t s1); /* 0x35de0 2 stack, callee cleanup */
void guest_fn_a8578_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d);



void decode_rle_rows_c1e3c(uint32_t dst);
void unit_owner_update_6b344(uint32_t unit);

uint32_t close_stream_record_bc4da(uint32_t rec, uint32_t extra);
void project_position_5ca88(uint32_t pos, uint32_t arg);
uint32_t guest_fn_15c0c4_x(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3); /* 0x15c0c4 4 stack, callee cleanup */

uint8_t load_resource_9cc28(uint32_t key, uint32_t buffer);
void run_script_conditions_a9694(uint32_t rec);
void guest_fn_a94e4_x(uint32_t rec, uint32_t edx_in);

void update_distance_minima_3237c(void);

void rotate_offset_c8d30(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1);
void rotate_offset_shift_c8d10(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1);

/* float_helpers9.c */
double sin_scaled_f8278(double angle);
double cos_scaled_f8298(double angle);

void combine_records_ef950(uint32_t a, uint32_t b, uint32_t out);
void combine_records_efbaf(uint32_t a, uint32_t b, uint32_t out);
void combine_records_efd84(uint32_t a, uint32_t b, uint32_t out);
void combine_record_value_ef97a(uint32_t a, uint32_t edx, uint32_t out, uint32_t ecx, uint32_t y0, uint32_t y1, uint32_t y2);
void feed_string_106d00(uint32_t obj, uint32_t str);
uint32_t record_type_size_8fdb8(uint32_t rec);
uint32_t skip_and_dispatch_1087cd(uint32_t obj);
void guest_fn_ef9aa_x(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, uint32_t esi_in, ef_addr32 out); /* 0xef9aa EAX/EDX/EBX/ECX/ESI; results EAX, EDX, SI stored as dword, dword, word at out */
void guest_fn_efbd9_x(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, uint32_t esi_in, ef_addr32 out); /* 0xefbd9, same convention */
void guest_fn_efdae_x(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, uint32_t esi_in, ef_addr32 out); /* 0xefdae, same convention */

void release_record_ranges_d6ffc(uint32_t list);
uint32_t seek_context_9cee8(uint32_t amount, uint32_t flags);
void add_scaled_vector_97914(uint32_t obj);
void walk_effect_records_8ba98(uint32_t rec);
void load_effect_list_8bad4(uint32_t first, uint32_t second);
void guest_fn_ea8cc_x(uint32_t vec, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t scale_bits); /* 0xea8cc EAX + one stack float, callee cleanup */

uint32_t mode_code_a7dac(void);
void format_state_bytes_82b6c(uint32_t buf);
void init_sound_entries_80374(uint32_t eax);

void enable_state_widget_362c4(void);
void pulse_delay_101688(int32_t n);
void append_entry_7ff0c(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1, uint32_t s2);
void write_node_position_ab060(uint32_t handle);
void guest_fn_a7318_2_x(uint32_t node, uint32_t key); /* 0xa7318 EAX/EDX */


void guest_fn_a7318_2_x(uint32_t node, uint32_t key); /* 0xa7318 EAX/EDX */

void send_key_request_35220(uint32_t key);
void update_widget_chain_7ca78(void);
void init_view_corners_c9ecc(void);
uint32_t queue_byte_118428(uint32_t obj, uint32_t ch);
void guest_fn_117b18_x(uint32_t obj); /* 0x117b18 EAX */



uint32_t layout_digits_11d322(uint32_t out, uint32_t src, int32_t intdigits, int32_t frac);

void right_align_number_11d385(uint32_t buf, int32_t len, int32_t width);
void advance_text_cursor_11451c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj, uint32_t out_col, uint32_t out_row);
void redraw_entries_a6038(void);

void integrate_motion_97950(uint32_t obj);
void flush_pending_a374c(uint32_t code);
uint32_t create_entry_d7764(uint32_t name, uint32_t b13, uint32_t b14, uint32_t w15, uint32_t w17, uint32_t d19, uint32_t b23);
uint32_t text_width_dd2f4(uint32_t str);

void project_position_5cb1c(uint32_t arg, int32_t x, int32_t y, int32_t z);
uint8_t within_unit_radius_66b74(uint32_t pos);
void draw_ratio_text_40624(void);
int32_t guest_fn_23760_x(ef_addr32 vec); /* 0x23760 EAX -> EAX */



void write_marker_file_286c0(void);
void append_entry_and_children_a499c(uint32_t id);
void run_command_loop_f4bdc(void);
void enable_state_widget_36340(void);
void draw_status_counter_48df8(void);

uint32_t step_animation_97860(uint32_t obj);
void release_runtime_buffers_1046c4(void);
void guest_fn_975f0_x(ef_addr32 record); /* 0x975f0 EAX */
uint32_t guest_fn_9783c_ret_x(ef_addr32 record, uint32_t count); /* 0x9783c EAX/EDX -> EAX */
uint32_t guest_fn_9775c_x(ef_addr32 record); /* 0x9775c EAX -> EAX */

void draw_glyph_6x5_eaf80(uint32_t colour, uint32_t x, uint32_t y, uint32_t glyph);
void draw_glyph_6x6_f2960(uint32_t colour, uint32_t x, uint32_t y, uint32_t glyph);
void unpack_planes_c38cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t src);
void reset_colour_tables_767f4(void);

void append_list_node_385cc(uint32_t idx, uint32_t byte_val, uint32_t val0, uint32_t val4, uint32_t s0, uint32_t s1);
void rescale_window_d8610(void);

uint8_t step_timer_entry_a1500(uint32_t idx);

void init_slot_record_b5640(uint32_t obj, uint32_t src, uint32_t ecx, uint32_t ebx, uint32_t need);
void send_param_block_f2a30(void);
void guest_fn_b54d0_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0); /* 0xb54d0 EAX/EDX/EBX/ECX + one stack argument, callee cleanup */

void draw_text_large_dd4c4(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);
void draw_text_medium_dd5bc(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);
void draw_text_small_dd6a0(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);

uint32_t tree_find_or_insert_de7d0(uint32_t text);
uint32_t tree_find_or_insert_9dbf0(uint32_t text);

uint32_t find_chunk_c1c24(uint32_t id);
void set_view_rect_c9f28(uint32_t obj);
void draw_clock_text_78934(void);

uint32_t read_archive_ebc50(uint32_t dst, uint32_t count, uint32_t flags, uint32_t ctx);
uint32_t read_bytes_9cde8(uint32_t dst, uint32_t count, uint32_t flags);

double tan_b45fa(double angle);
double atan_dc1d4(double value);
double table_double_11ac3c(uint32_t obj, uint32_t a, uint32_t b, uint32_t c);
double modf_10d9c6(double x, uint32_t ip);
double scaled_call_11b588(double x);
double combine_11bf00(uint32_t v, double a0, double a1);
double guest_fn_11b540_x(double x); /* 0x11b540 one double on the stack, callee cleanup, result in ST0 */

double atan2_dc1e9(double st0, double st1);
double abs_area_11af60(uint32_t p);
double slope_angle_11b0ec(uint32_t p);
double floor_e86f7(double x);

void build_extent_record_10a030(uint32_t dst, uint32_t src);
void scale_vector3_ea8cc(uint32_t vec, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t scale_bits);
double blend_angle_11aff4(uint32_t p, double d);
double scaled_ratio_11bfc8(uint32_t p, double a0, double a1);
double scaled_call_11b540(double x);
double guest_fn_11af14_x(double x); /* 0x11af14 one double on the stack, callee cleanup, result in ST0 */



void update_globals_1198ec(void);
double sqrt_branch_11af14(double a);
void fdiv_bug_probe_11016c(void);

void build_segment_record_10a078(uint32_t dst, uint32_t src);
void derive_coefficients_10a1c4(uint32_t dst, uint32_t src);

double pow_b3fec(double x, double y);
double guest_fn_fa362_x(double x); /* 0xfa362 ST0 -> ST0, natural logarithm */
double guest_fn_dcb96_x(double x); /* 0xdcb96 ST0 -> ST0, exponential */
void guest_callptr7_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g); /* indirect cdecl call with seven stack arguments */



double falloff_11bbdc(double a0, double a1);
void project_triple_11c484(uint32_t dst, uint32_t src);

void update_slot_texts_78a0c(void);
void dispatch_dialog_state_34b2c(void);
uint32_t shrink_counter_bd7cc(uint32_t p, uint32_t edx);

void draw_glyph_7x9_e5e90(uint32_t x, uint32_t y, uint32_t code);
uint8_t best_slot_kind_6f6b8(uint32_t unit);

void load_string_table_e3868(void);

void circle_walk_down_fdec0(int32_t x, int32_t y, int32_t err, int32_t y_end, uint32_t fn, uint32_t ctx);
void circle_walk_up_fde20(int32_t x, int32_t y, int32_t err, int32_t y_end, uint32_t fn, uint32_t ctx);

void init_pattern_buffer_e40e4(void);
void process_render_object_d23d8(uint32_t obj);
void guest_fn_d2130_x(ef_addr32 obj); /* 0xd2130 EAX */
void guest_fn_d2320_x(ef_addr32 obj); /* 0xd2320 EAX */



uint8_t class_selector_code_b9e9c(uint32_t obj, uint32_t sel);
void finish_resource_file_e3444(void);
void guest_fn_e36bc_x(void); /* 0xe36bc no arguments */

void restore_strip_98ac0(uint32_t a);
uint32_t format_signed_11d27b(uint32_t out, int32_t value, int32_t width, int32_t room);
uint32_t add_progress_cd2e8(uint32_t delta);
int32_t parse_number_103e5c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t str, int32_t base, uint32_t endp);
uint8_t classify_cell_85bc8(ef_addr32 pos);

uint8_t init_mouse_ddbe0(void);
void guest_fn_ddce8_x(void); /* 0xddce8 unknown, no arguments */

void init_once_101a90(void);
ef_addr32 alloc_stream_slot_fa6d3(void);

void blit_transparent_cd957(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t skip, uint32_t rows, uint32_t width, int32_t y, int32_t x, ef_addr32 src);
void save_and_draw_strip_98b68(uint32_t x, uint32_t y);
void finish_log_d1bec(void);
void draw_cell_text_5e1b0(int32_t a, int32_t b, ef_addr32 fmt, int32_t c);
void draw_marked_box_3bcc0(int32_t a, int32_t b, ef_addr32 ptr, int32_t c, uint32_t mode);
void guest_fn_5e21c_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x5e21c EAX/EDX/EBX/ECX */
void guest_fn_c8ccc_x(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0xc8ccc 5 stack, callee cleanup */

void round_double_to_int64_10e28a(ef_addr32 p);
void show_main_widgets_7cc5c(void);
void guest_fn_7bee8_x(uint32_t a); /* 0x7bee8 EAX */

void write_log_d1c54(ef_addr32 name, ef_addr32 src, int32_t mode);
void classify_double_110130(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1, uint32_t s2);
void float_to_extended_f017a(uint32_t bits, ef_addr32 out);
void decimal_string_to_double_ebdcc(ef_addr32 text, ef_addr32 out);
void guest_fn_d1afc_x(void); /* 0xd1afc unknown, no arguments */

void segread_9be0d(ef_addr32 out);
void int386x_core_9c47a(uint32_t intno, uint32_t in, uint32_t out, uint32_t sregs);
uint32_t int386x_9c461(uint32_t intno, uint32_t in, uint32_t out, uint32_t sregs);
uint32_t int386_9c446(uint32_t intno, uint32_t in, uint32_t out);
uint32_t guest_seg_x(uint32_t which); /* segment register selector: 0 ES, 1 CS, 2 SS, 3 DS, 4 FS, 5 GS */
void guest_dos_int_x(uint32_t intno, uint32_t in, uint32_t out, uint32_t sregs); /* software interrupt through the stub table at 0x9c558 */

uint32_t run_steps_d6694(void);
uint8_t try_open_slot_107004(ef_addr32 a, ef_addr32 out);
void hash_and_run_ea20c(ef_addr32 obj, ef_addr32 text);
uint32_t guest_fn_d66c2_x(void); /* 0xd66c2 unknown, no arguments, result in EAX */
uint32_t guest_fn_106f10_x(uint32_t a, uint32_t b); /* 0x106f10 EAX/EDX -> AL */
void guest_fn_10d9e3_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0x10d9e3 EAX/EDX/EBX/ECX */

uint32_t classify_record_113b78(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 ctx);
void reload_text_file_f819c(ef_addr32 name);
void update_bearing_9f664(int32_t index);
void guest_callptr3_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c); /* indirect cdecl call with three stack arguments */

uint32_t select_wide_slot_count_fae30(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);
uint32_t select_wide_slot_count_fb0d4(uint32_t sel, uint32_t cls, uint32_t divisor, uint32_t list);
uint32_t select_slot_count_by_variant_fb1a0(uint32_t sel, uint32_t cls, uint32_t variant, uint32_t divisor, uint32_t list);

void add_time_offset_b54d0(ef_addr32 hours, ef_addr32 minutes, ef_addr32 seconds, int32_t rate, uint32_t rate_code);
void refresh_current_object_598e8(void);
void guest_fn_5a2d0_x(void); /* 0x5a2d0 unknown, no arguments */
void guest_fn_5a220_x(void); /* 0x5a220 unknown, no arguments */
void guest_fn_5a468_x(void); /* 0x5a468 unknown, no arguments */
void guest_fn_5e87c_x(uint32_t a); /* 0x5e87c EAX */
void guest_fn_5c518_x(void); /* 0x5c518 unknown, no arguments */
void guest_fn_59fa4_x(void); /* 0x59fa4 unknown, no arguments */
void guest_fn_59998_x(void); /* 0x59998 unknown, no arguments */

void advance_object_vectors_a0174(ef_addr32 obj);
void step_demo_script_1a3cc(void);
void load_list_names_aa3dc(void);

void count_matching_tiles_d3844(ef_addr32 obj);
void update_grid_nodes_2a7f8(void);
uint32_t register_trace_block_d5e0c(ef_addr32 rec);

void add_direction_offset_cee88(ef_addr32 vec, uint32_t a, uint32_t b, int32_t k);
void set_clip_rect_c94f4(uint32_t colour, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void prepare_shape_list_d2320(ef_addr32 obj);



void clip_rect_to_screen_e58a8(void);
void select_button_state_4f718(uint32_t idx, uint32_t mode);
void decode_run_literal_e330c(uint32_t fill, uint32_t dst, uint32_t count);

void teardown_once_106b1c(void);
void mark_matching_slots_35f04(void);
uint32_t start_trace_d5d18(uint32_t a);

void init_once_105230(void);
void set_slot_state_50b68(uint32_t index, uint32_t kind);
uint32_t point_in_triangle_cf960(ef_addr32 tri, int32_t px, int32_t py);
void transform_and_relate_9f710(int32_t a, uint32_t y, int32_t z, int32_t c);
void guest_fn_117ace_x(void); /* 0x117ace unknown, no arguments */
void guest_callptr3r_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c); /* indirect call: EAX/EDX/EBX register arguments */

void draw_glyph_7x7_e5cf0(int32_t x, int32_t y, int32_t glyph);

uint32_t rle_encode_d1924(ef_addr32 dst, ef_addr32 src);
ef_addr32 find_entry_in_range_5ee80(void);
void claim_ui_slot_801e4(int32_t index, uint32_t arg, uint32_t flag);

void draw_shadowed_text_48c90(uint32_t color, int32_t x, int32_t y, ef_addr32 fmt, ...);
void read_pair_list_f314c(void);



void fill_rect_clipped_cec00(uint32_t color, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

void insert_nearest_65570(ef_addr32 rec);

void write_string_table_e36bc(void);

void scan_triggers_3d950(void);
void bind_object_slot_6e74(uint32_t id);

uint32_t heading_to_current_object_66fc(void);
uint32_t lookup_plane_cell_7ae88(uint32_t mode, int32_t x, uint32_t y, int32_t z);
void guest_fn_84e3c_x(uint32_t a, uint32_t b, uint32_t c); /* 0x84e3c EAX/EDX/EBX */

uint8_t setup_line_query_28aa8(ef_addr32 a, ef_addr32 b, uint32_t kind);
uint32_t find_entry_at_cursor_11014(ef_addr32 obj);

void build_offset_rings_5ae70(void);
uint32_t lookup_plane_cell_7af60(uint32_t mode, int32_t x, uint32_t y, int32_t z);
uint32_t open_numbered_log_9a544(void);
uint32_t guest_fn_d0278_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xd0278 EAX/EDX/EBX/ECX -> EAX */

void set_record_state_14c5c(int32_t idx, uint32_t state);
void guest_fn_14d58_x(uint32_t a); /* 0x14d58 EAX */

void draw_text_font0_dd788(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);
void draw_text_font6_dd370(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);
void draw_text_upper_dd26c(uint32_t colour, uint32_t x, uint32_t y, uint32_t str);
void draw_text_by_font_a8578(int32_t x, int32_t y, uint32_t colour, uint32_t text);



ef_addr32 strcat_9ca57(ef_addr32 dst, ef_addr32 src);
void outp_117b0c(uint32_t port, uint32_t value);
uint32_t inp_9c430(uint32_t port);
ef_addr32 memcpy_dc832(ef_addr32 dst, ef_addr32 src, uint32_t count);
ef_addr32 getenv_9c858(ef_addr32 name);
void transpose_matrix3_d099c(ef_addr32 dst, ef_addr32 src);
void guest_outp_x(uint32_t port, uint32_t value); /* OUT DX, AL */
uint32_t guest_inp_x(uint32_t port); /* IN AL, DX */

void wait_vsync_dc2e0(void);
uint32_t lseek_fa95f(uint32_t handle, uint32_t offset, uint32_t whence);
void intdos_2b0(uint32_t in, uint32_t out);
uint32_t dos_range_104840(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t first, uint32_t last);
uint32_t dos_range_1048b4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t first, uint32_t last);
void guest_fn_1101e8_x(uint32_t a); /* 0x1101e8 EAX */
void guest_dos_intdos_x(uint32_t in, uint32_t out, uint32_t sregs); /* int 21h without storing the flags */

void run_page_text_9e508(int32_t page, uint32_t index);

int32_t sum_char_table_diff_bef2c(uint32_t a, uint32_t b, uint32_t obj);
void format_and_enter_eb2c0(ef_addr32 fmt, ...);
void copy_rows_eb174(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                     uint32_t x, uint32_t y, uint32_t len, uint32_t count,
                     uint32_t step, uint32_t x0, uint32_t rows, uint32_t base);
uint32_t guest_fn_fb87c_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* 0xfb87c EAX/EDX/EBX/ECX */

void draw_cursor_ticks_5c9d4(uint32_t obj, uint32_t arg);
void load_record_file_ca200(uint32_t name);
void guest_fn_39a0c_x(uint32_t a, uint32_t b); /* 0x39a0c EAX/EDX */
void guest_fn_dec18_x(uint32_t a, uint32_t b, uint32_t c); /* 0xdec18 EAX/EDX/EBX */
uint32_t guest_callptr_eaxedx_ret_x(ef_addr32 fn, uint32_t a, uint32_t b); /* indirect call, arguments in EAX and EDX, result in EAX */

uint32_t find_node_b4db0(uint32_t head_a, uint32_t head_b, uint32_t kind, uint32_t param);

void try_take_target_89ac0(uint32_t obj);
uint32_t guest_fn_89b58_x(uint32_t obj); /* 0x89b58 EAX -> AL */

uint8_t check_data_file_e3b7c(uint32_t name);
void fill_diamond_e2ba0(uint32_t color, uint32_t x, uint32_t y, uint32_t size);

void draw_entry_boxes_e8484(int32_t key, uint32_t x, uint32_t y, uint32_t z);

void retire_unit_8d330(int32_t index);
void guest_fn_8ca24_x(uint32_t obj, int32_t value); /* 0x8ca24 EAX/EDX */

uint32_t pump_records_eab44(void);
void guest_fn_f9ee0_x(uint32_t a, uint32_t b); /* 0xf9ee0 2 stack, callee cleanup */
void guest_fn_10dfec_x(uint32_t a, uint32_t b); /* 0x10dfec 2 stack, callee cleanup */

void draw_clipped_text_e605c(uint32_t color, int32_t x, int32_t y, uint32_t str);
void guest_fn_e5f64_x(uint32_t x, uint32_t y, uint32_t code); /* 0xe5f64 EAX/EDX/EBX */

void reset_stream_block_106644(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t p);

uint32_t remaining_share_7fc34(uint32_t scale, uint32_t index);
uint32_t write_status_lines_101ba8(void);
void update_remote_view_f2ac0(void);
void guest_fn_a7bd4_x(void); /* 0xa7bd4 */

void draw_clipped_text_e5db4(uint32_t color, int32_t x, int32_t y, uint32_t str);

int32_t classify_object_b4ee0(uint32_t obj);

void draw_prop_text_15a14(uint32_t color, int32_t x, int32_t y, uint32_t str);

void show_splash_c90e0(void);

void draw_planar_sprite_cdd18(uint32_t dst, uint32_t src, uint32_t xmin, uint32_t xmax, uint32_t ymin, uint32_t ymax);

void compute_view_corners_d8528(void);

int32_t dos_date_request_dde80(uint32_t value);

void set_unit_target_73734(int32_t a, int32_t b);

void update_group_flags_cb6fc(uint32_t head, uint32_t arg);
uint32_t guest_fn_cc1ec_x(uint32_t a, uint32_t b, uint32_t c); /* 0xcc1ec EAX/EDX/EBX -> AX */
uint32_t guest_fn_b0954_x(uint32_t a, uint32_t b, uint32_t c); /* 0xb0954 EAX/EDX/EBX -> AX */
void guest_fn_cc188_x(uint32_t a, uint32_t b, uint32_t c); /* 0xcc188 EAX/EDX/EBX */
void guest_fn_cc164_x(uint32_t a, uint32_t b, uint32_t c); /* 0xcc164 EAX/EDX/EBX */

void draw_entry_list_78848(void);

void insert_ordered_id_a48ac(uint32_t id, uint32_t after);

void compute_unit_distance_39cb4(uint32_t out, uint32_t out_pct);
uint32_t guest_fn_86968_x(uint32_t value, uint32_t out); /* 0x86968 EAX/EDX -> AL */

void accumulate_samples_a35bc(void);
void guest_fn_a3e50_x(uint32_t buf, uint32_t code, uint32_t len); /* 0xa3e50 EAX/EDX/EBX */

void rebuild_group_tables_49bdc(uint32_t p);

void draw_glyph_clipped_e5f64(int32_t x, int32_t y, uint32_t code);

void tally_roster_kinds_50658(void);

void reset_remote_cursor_f2b7c(void);

void compact_slots_3739c(int32_t unit);
uint32_t guest_fn_8ffe8_x(uint32_t id); /* 0x8ffe8 EAX -> EAX */

uint32_t map_key_to_command_8ae6c(uint32_t key);

void start_script_record_a060c(int32_t n);
void start_script_record_9fb00(int32_t n);
void guest_callptr4_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* indirect call: EAX/EDX/EBX/ECX register arguments */
void guest_fn_160255_x(void); /* 0x160255 */

uint32_t make_route_record_fb954(uint32_t a, uint32_t b, uint32_t c);

void dispatch_pending_key_1a47c(void);
void guest_fn_11de0_x(uint32_t key); /* 0x11de0 EAX */

void find_letter_entry_11de0(uint32_t key);

void credit_hit_74480(int32_t a);

void blend_palette_757bc(int32_t t, uint32_t to, uint32_t from, uint32_t dest, uint32_t count, uint32_t start);

void register_ranges_111c90(void);

void randomize_pilot_779f4(uint32_t obj);

void step_animation_script_a207c(uint32_t obj, uint32_t other);

uint32_t pack_bit_planes_d19ec(uint32_t dst, uint32_t src);

void assign_roster_slot_740e4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t slot);

void reset_settings_104580(void);
void guest_fn_110fd4_x(void); /* 0x110fd4 (tail jump) */

void init_video_mode_cdb20(void);
void guest_fn_fe710_s(uint32_t mode); /* 0xfe710 1 stack argument, callee cleanup */
void guest_fn_fc39c_s(uint32_t a, uint32_t b); /* 0xfc39c 2 stack arguments, callee cleanup */

void dump_page_header_9ab6c(uint32_t stream);

void init_roster_state_77a8(uint32_t n);
uint32_t guest_fn_7780_x(uint32_t count, uint32_t size); /* 0x7780 EAX/EDX -> EAX */

uint32_t traced_lookup_102028(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t p1, uint32_t p2);
uint32_t guest_fn_103f04_x(uint32_t a, uint32_t b); /* 0x103f04 cdecl, 2 stack arguments */

void draw_tactical_header_3b58(void);
void guest_fn_3ea8_x(void); /* 0x3ea8 */
void guest_fn_a4342_va(ef_addr32 dest, ef_addr32 format, ...); /* 0xa4342 cdecl, variadic */

void update_view_stages_45574(uint32_t a, uint32_t s, uint32_t b);


void guest_fn_433fc_x(uint32_t a, uint32_t b); /* 0x433fc EAX/EDX */
void guest_fn_4357c_x(uint32_t a, uint32_t b); /* 0x4357c EAX/EDX */
void guest_fn_436dc_x(uint32_t a, uint32_t b); /* 0x436dc EAX/EDX */
void guest_fn_43b14_x(uint32_t a, uint32_t b); /* 0x43b14 EAX/EDX */
void guest_fn_43c84_x(uint32_t a, uint32_t b); /* 0x43c84 EAX/EDX */
void guest_fn_43df4_x(uint32_t a, uint32_t b); /* 0x43df4 EAX/EDX */
void guest_fn_43f48_x(uint32_t a, uint32_t b); /* 0x43f48 EAX/EDX */
void guest_fn_441fc_x(uint32_t a, uint32_t b); /* 0x441fc EAX/EDX */
void guest_fn_443fc_x(uint32_t a, uint32_t b); /* 0x443fc EAX/EDX */
void guest_fn_447ec_x(uint32_t a, uint32_t b); /* 0x447ec EAX/EDX */
void guest_fn_4496c_x(uint32_t a, uint32_t b); /* 0x4496c EAX/EDX */
void guest_fn_44aec_x(uint32_t a, uint32_t b); /* 0x44aec EAX/EDX */
void guest_fn_44c68_x(uint32_t a, uint32_t b); /* 0x44c68 EAX/EDX */
void guest_fn_44de8_x(uint32_t a, uint32_t b); /* 0x44de8 EAX/EDX */
void guest_fn_44f68_x(uint32_t a, uint32_t b); /* 0x44f68 EAX/EDX */
void guest_fn_450f4_x(uint32_t a, uint32_t b); /* 0x450f4 EAX/EDX */
void guest_fn_45274_x(uint32_t a, uint32_t b); /* 0x45274 EAX/EDX */
void guest_fn_453f4_x(uint32_t a, uint32_t b); /* 0x453f4 EAX/EDX */

void redraw_item_a5ef8(uint32_t id);
uint32_t guest_fn_e5870_x(uint32_t record); /* 0xe5870 EAX -> AL */
void guest_fn_a5b70_x(uint32_t record); /* 0xa5b70 EAX */
void guest_fn_a5ca4_x(uint32_t record); /* 0xa5ca4 EAX */

void draw_mission_clock_78e1c(void);

void advance_follow_slot_3a148(int32_t a);
uint32_t guest_fn_3a268_x(uint32_t v); /* 0x3a268 EAX -> AL */
void guest_fn_39edc_x(uint32_t a, uint32_t v); /* 0x39edc EAX/EDX */
void guest_fn_3a000_x(uint32_t a); /* 0x3a000 EAX */

void add_recent_file_ac2e4(uint32_t name, uint32_t flag);

void tick_broadcast_ring_56318(void);
void guest_fn_a3f74_x(uint32_t a, uint32_t b); /* 0xa3f74 EAX/EDX */

uint8_t find_nearest_target_89698(uint32_t obj);
uint32_t guest_fn_906b4_x(uint32_t v); /* 0x906b4 EAX -> AL */
uint32_t guest_fn_89060_x(uint32_t obj, uint32_t unit); /* 0x89060 EAX/EDX -> AL */

uint32_t open_resource_9ccb8(uint32_t name);
uint32_t guest_fn_ebb58_x(uint32_t name, uint32_t archive); /* 0xebb58 EAX/EDX */
void guest_fn_a7ac8_x(uint32_t handle, uint32_t offset); /* 0xa7ac8 2 stack arguments, callee cleanup */

uint32_t count_typed_nodes_b0b00(uint32_t kind, uint32_t head, uint32_t arg);
uint32_t guest_fn_bfcd8_x(uint32_t node, uint32_t arg); /* 0xbfcd8 EAX/EDX -> EAX */

void skip_to_section_f3a1c(uint32_t name);

void load_object_geometry_a6808(uint32_t rec);

void fill_selection_dialog_4db60(void);

void nudge_view_orientation_91df4(void);

void serial_interrupt_117bfc(uint32_t rec);
void guest_fn_117b18_x(uint32_t rec); /* 0x117b18 EAX */

void format_event_message_57820(uint32_t rec);

void update_camera_position_319ac(void);

void bind_unit_entry_76b80(uint32_t id);

void advance_playlist_a94e4(uint32_t list);



void sort_nodes_by_order_e6848(uint32_t order, uint32_t count, uint32_t head_ptr, uint32_t key);

void draw_table_sprite_cde00(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t y, uint32_t x, uint32_t idx, uint32_t table);

uint32_t assign_unit_object_6abac(uint32_t id, uint32_t k);

void install_render_table_e1570(void);

void run_top_script_f4a88(void);

uint32_t check_position_hit_777f4(uint32_t pos);
void guest_fn_106b0_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0); /* 0x106b0 EAX/EDX/EBX/ECX + 1 stack, ret 4 */

void latch_control_bits_84788(uint32_t rec);

void draw_map_markers_436dc(uint32_t head, uint32_t list);



void start_sample_d7158(uint32_t chan, uint32_t pan, uint32_t rec, uint32_t vol);

void projectile_ground_test_1df90(uint32_t rec);
void guest_fn_110ec_x(uint32_t a, uint32_t b); /* 0x110ec EAX/EDX */



void draw_map_markers_4357c(uint32_t head, uint32_t list);

void init_event_record_bb324(uint32_t rec, uint32_t kind, uint32_t arg);

uint32_t get_video_mode_fe700(void);
void set_patch_dword_a45e4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value);

void reset_marker_1111ca(void);
void check_marker_1111b1(void);
uint32_t largest_free_block_eb2a8(void);
void set_rm_vector_111142(uint32_t num, uint32_t value);
void poll_serial_dff4c(void);
void guest_fn_dfac8_x(void); /* 0xdfac8 */

void send_keyboard_byte_cd3c0(uint32_t value);
void set_keyboard_leds_cd414(uint32_t mask);
uint32_t read_console_char_11d63f(void);

uint32_t is_device_handle_1101be(uint32_t handle);
uint32_t delete_file_fe541(uint32_t name);
uint32_t dos_read_10f0e8(uint32_t handle, uint32_t buffer, uint32_t count);
uint32_t set_and_check_video_mode_fe710(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t mode);
uint32_t dos_alloc_eb288(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t paragraphs);
void copy_dta_out_ded1a(uint32_t u0, uint32_t dst);
void copy_dta_in_ded3d(uint32_t u0, uint32_t src);

uint32_t strnlen_far_ed542(uint32_t offset, uint32_t seg, uint32_t max);

void divmod_store_ea49a(int32_t dividend, int32_t divisor, ef_addr32 result);
void put_pixel_fedcc(uint32_t row, uint32_t x, uint32_t colour);

void put_block2_feddc(uint32_t row, uint32_t x, uint32_t colour);
void shift_triples_c59b7(ef_addr32 src, ef_addr32 dst, uint32_t shift);

void divide_table_entries_c5409(uint32_t div, uint32_t index);
void multiply_table_entries_c5441(uint32_t mul, uint32_t index);

uint32_t fetch_scaled_value_c54ef(uint32_t index);

void select_shade_table_c47e0(uint32_t index);

void load_vertices_c48dc(ef_addr32 src, uint32_t count);

double box_shape_ratio_11afb4(uint32_t box);

uint32_t project_point_pair_c737c(ef_addr32 p1, ef_addr32 p2, ef_addr32 dst);

void update_counter_widget_4e7b8(uint32_t show);

uint32_t dos_alloc_block_1047e4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                                uint32_t paragraphs, uint32_t blocks_out, uint32_t segment_out, uint32_t error_out);

void blit_masked_cd82c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                       uint32_t y, uint32_t x, uint32_t height, uint32_t width,
                       uint32_t src_y, uint32_t src_x, uint32_t src);

uint32_t report_formatted_e8784(ef_addr32 fmt, ...);
void guest_callptr4c_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* indirect cdecl call with four stack arguments, caller cleanup */

void push_message_entry_56268(ef_addr32 src);

void u64_to_digits_10e2f5(ef_addr32 value_ptr, ef_addr32 buf, uint32_t n);

uint32_t bsearch_10d9e3(uint32_t key, uint32_t base, uint32_t n, uint32_t size, uint32_t cmp);
void set_scale_pair_c486c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value);

void unpack_settings_3519c(ef_addr32 src);



void poll_modem_status_df738(void);

void select_animation_11458c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t rec, uint32_t key);

void reposition_camera_31774(void);
void serial_status_dfeb8(void);

void send_command_packet_55f54(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                               uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3,
                               uint32_t s4, uint32_t s5, uint32_t s6);

void draw_count_icon_4d24c(uint32_t x, uint32_t y, uint32_t rec, uint32_t count);

uint32_t reserve_entries_81394(uint32_t unused, uint32_t list, uint32_t sum);

void init_weapon_slots_8ca24(uint32_t rec, uint32_t index);

void open_stream_369fc(uint32_t name, uint32_t mode);



uint32_t print_matching_records_b2758(uint32_t x, uint32_t y, uint32_t max, uint32_t head_ptr, uint32_t out);

void build_render_object_d2584(ef_addr32 list, uint32_t count);

void load_text_file_510d0(void);

void draw_label_entries_150d4(void);

void setup_external_view_95704(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);

void draw_icon_1e6f0(uint32_t color, uint32_t shadow, uint32_t idx, uint32_t kind, uint32_t x, uint32_t y);

uint32_t fopen_core_bc287(uint32_t name, uint32_t mode, uint32_t file, uint32_t share);



int32_t find_icon_at_4dee0(uint32_t x, uint32_t y);

int32_t setup_edge_step_1014a4(uint32_t out, uint32_t step, uint32_t p0, uint32_t p1);

void project_vertices_fc8cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t count, uint32_t list);

void build_textured_object_d2634(ef_addr32 list, uint32_t count);

void add_dirty_rect_e51c4(uint32_t a, uint32_t c, uint32_t b, uint32_t d, uint32_t add);

void find_lowest_rank_a38f8(void);

void assign_table_slot_8fe4(uint32_t index, uint32_t table);

void build_rotation_words_dc43e(uint32_t ebx, uint32_t esi, uint32_t edi);

void format_fixed_ed5ea(uint32_t buf, int32_t value, uint32_t fmt);

double eval_binary_record_10e4dd(uint32_t flags, uint32_t a, uint32_t b);

uint32_t find_nearest_object_2c454(uint32_t pos, uint32_t id, uint32_t side, uint32_t type);

uint8_t open_resource_file_e378c(uint32_t name);

void normalise_angles_6bca8(void);

void build_unit_name_7548c(uint32_t dest, uint32_t index);

uint32_t store_scaled_vectors_7b08c(uint32_t a, uint32_t b);

void update_stock_e6d78(void);

void apply_default_record_1132b8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj, uint32_t rec);

void scan_nodes_near_unit_18a68(void);

void load_dataset_files_c8e58(void);

void open_named_data_a81e4(uint32_t name, uint32_t src);

void draw_effect_records_80c3c(void);

void prepare_briefing_file_35cfc(void);

void update_proximity_39874(uint32_t x, uint32_t y, uint32_t z);

void draw_nearby_entries_87fe4(void);

void setup_viewport_window_76620(void);

uint32_t register_resource_9dcf8(uint32_t name, uint32_t text);

void append_table_entry_16018a(void);
void guest_fn_c565e_x(uint32_t esi); /* 0xc565e ESI = slot index */
void guest_fn_c552e_x(uint32_t esi); /* 0xc552e ESI = slot index */

double scale_by_table_ebee4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double x, int32_t n);
void compute_turn_rate_6eaf4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double arg);

double estimate_coefficient_6c1f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double x);

uint32_t strstr_e7d27(uint32_t hay, uint32_t needle);

void fill_span_d4510(void);

uint32_t dos_get_time_f10ca(uint32_t tm);

uint32_t build_ramp_table_e7ea4(int32_t lo, int32_t hi);

uint32_t read_hires_timer_fe560(void);

uint8_t store_inverse_ratio_d6cac(uint32_t out);

void matrix_multiply_d08fc(uint32_t out, uint32_t a, uint32_t b);

void normalise_globals_109a80(void);

void draw_dithered_span_d4430(void);

uint32_t extended_to_float_f0034(uint32_t ptr);

uint32_t double_bits_to_float_118b10(uint32_t lo, uint32_t hi);
void draw_shaded_span_d44b0(void);

uint32_t project_if_outside_c46f8(uint32_t margin, uint32_t rec);

void send_guarded_message_103e0c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj);

uint32_t checksum_bytes_10dc1d(uint32_t p, uint32_t count);
void swap_buffers_ded60(uint32_t a, uint32_t b, uint32_t count);

void build_row_table_fc39c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t count, uint32_t step);
uint32_t handler_address_117dd8(uint32_t index);

uint32_t project_relative_point_c89cc(uint32_t src, uint32_t out, uint32_t dst);
uint32_t project_offset_point_c89f4(uint32_t src, uint32_t out, uint32_t dst);
uint32_t checksum_bytes_fa2e4(uint32_t p, uint32_t count);

void set_params_and_run_c4654(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a, uint32_t b, uint32_t c);
void guest_fn_e7a7c_x(void); /* 0xe7a7c */

uint32_t view_volume_half_c473c(uint32_t margin, uint32_t x, uint32_t y, uint32_t z);
void split_words_10e08d(uint32_t src, uint32_t dst, uint32_t count);

void set_object_target_26670(uint32_t index, uint32_t x, uint32_t y, uint32_t z);

uint32_t alloc_zeroed_cc6c0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t size, uint32_t fmt, ...);

void flush_pending_slots_112808(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj);

void read_script_line_f4640(void);

void load_resource_list_d5f0c(uint32_t list);

void setup_camera_955c0(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0);

void load_fixed_files_c8f38(void);

void show_preview_picture_41314(uint32_t name);

void start_timer_service_e28e8(void);

void assign_group_slots_49fd4(uint32_t p);

void insert_midpoint_4e5bc(void);

void advance_position_cef3c(uint32_t v, uint32_t ang, uint32_t scale);

void draw_displaced_span_ffe78(void);

void build_render_list_d2500(uint32_t list, uint32_t count);
void build_render_list_d2704(uint32_t list, uint32_t count);

void build_render_list_d2c84(uint32_t list, uint32_t count);
void build_render_list_d2d7c(uint32_t list, uint32_t count);

void advance_view_queue_91c98(void);

void queue_message_47764(uint32_t di, uint32_t si, uint32_t ebx, uint32_t ecx, uint32_t s0);

void refresh_list_widget_348e4(void);
void guest_fn_a7588_x(ef_addr32 a, ef_addr32 b); /* 0xa7588 EAX/EDX */

void draw_table_sprite_440f4(uint32_t sel, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1, uint32_t esi_in);

uint32_t next_list_token_8b6e0(uint32_t mode, uint32_t prefix, uint32_t out);

void redraw_widget_a60a8(uint32_t obj);
uint32_t guest_fn_e588c_ret_x(ef_addr32 record); /* 0xe588c EAX -> EAX */

void draw_heading_marker_5de14(uint32_t x, uint32_t y, uint32_t size, uint32_t obj);



void update_item_dialog_34298(void);

void find_data_size_c3364(uint32_t buf, uint32_t name);

void layout_slot_widgets_c768(uint32_t index);

void load_pattern_file_e4010(uint32_t name);
void fill_button_widgets_4f7d0(void);

void read_selector_ab174(uint32_t handle);

void poll_mouse_ddae0(void);
void log_message_ea50c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t fmt, ...);

void refresh_button_dialog_4f8a8(void);

void show_splash_picture_f41dc(uint32_t name);

void copy_last_token_abe44(ef_addr32 str);
uint32_t extended_to_int_eff04(ef_addr32 p);

uint32_t compare_extended_efb16(ef_addr32 a, ef_addr32 b);

void offset_vector_by_angles_ceda0(ef_addr32 vec, ef_addr32 ang, uint32_t scale);

void build_mode_label_326d8(void);

void fetch_entry_pair_84e3c(ef_addr32 out, uint32_t unit, uint32_t sub);

void queue_record_entry_9f850(uint32_t index);

void handle_event_record_a348c(uint32_t index);

uint32_t run_choice_prompt_7a194(void);

void send_mouse_requests_de3b4(uint32_t start);

void draw_framed_panel_38be8(uint32_t id, uint32_t y, uint32_t x);

void load_colour_table_107310(void);
void release_locked_ranges_114ad8(void);

void close_widget_a4b80(uint32_t id);

uint32_t pick_roster_slot_7bc0(uint32_t unit, uint32_t group);

void count_side_tallies_9b668(void);

uint32_t find_paired_node_b49b4(ef_addr32 a, ef_addr32 b, uint32_t group, ef_addr32 list);

void uncount_side_tallies_9b780(void);

void claim_idle_nodes_be208(uint32_t owner, uint32_t target, uint32_t value, ef_addr32 rec, ef_addr32 headptr);

void refresh_node_blocks_caea0(ef_addr32 headptr, ef_addr32 table, uint32_t count);

void apply_pending_notices_6563c(void);

void decode_column_sprite_110a08(ef_addr32 src, ef_addr32 dst, uint32_t stride);

void draw_shadow_box_f3904(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

void draw_arc_fe380(uint32_t colour, uint32_t cy, uint32_t cx, uint32_t lo, uint32_t hi, uint32_t radius);

void fill_dialog_labels_4cff4(void);

void draw_arc_by_length_fe49c(uint32_t colour, uint32_t cy, uint32_t cx, uint32_t lo, uint32_t hi, uint32_t radius);

void update_notice_list_654b8(void);

void refresh_status_widgets_d550(void);

int32_t atan2_fixed_11f368(int32_t a, int32_t b);

uint32_t start_stream_ebb58(uint32_t key, ef_addr32 obj);

void select_mode_byte_9338(uint32_t kind);

uint32_t start_voice_sample_d72b4(uint32_t idx, ef_addr32 obj, uint32_t size);

ef_addr32 create_slot_record_fc0ec(uint32_t a, uint32_t b);

void build_glyph_runs_e17f8(void);

uint32_t find_object_near_90940(ef_addr32 pos);

void refresh_unit_roster_76e2c(uint32_t a, uint32_t b);

void poll_pointer_device_ac854(void);

void draw_outline_5af50(uint32_t n);

ef_addr32 find_group_node_b4aec(ef_addr32 a, ef_addr32 b, uint32_t group, ef_addr32 list);

void open_resource_by_name_ad440(ef_addr32 name, ef_addr32 out);
uint32_t write_page_record_a1d50(void);
void select_page_a1d00(uint32_t id);
uint32_t guest_fn_3cd40_x(ef_addr32 name); /* 0x3cd40 EAX -> EAX */

uint32_t mktime_f118d(ef_addr32 tm);
uint32_t get_time_seconds_a4574(ef_addr32 out);
void guest_fn_10eda1_x(void); /* 0x10eda1 */
void guest_fn_10e97f_x(ef_addr32 tm); /* 0x10e97f EAX */
uint32_t guest_fn_f118d_x(ef_addr32 tm); /* 0xf118d EAX -> EAX */



void queue_grid_node_2a8ac(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, ef_addr32 s1);

void rebuild_page_c868(void);
void guest_fn_cccc_x(ef_addr32 w); /* 0xcccc EAX */

void apply_order_record_56890(ef_addr32 rec);
void guest_fn_10a9c_x(int32_t a, int32_t b, int32_t c); /* 0x10a9c EAX/EDX/EBX */

void define_text_slot_e3e50(ef_addr32 text);

void recompute_bounds_31d7c(void);
void guest_fn_324b0_x(ef_addr32 a, ef_addr32 b, ef_addr32 c, ef_addr32 out); /* 0x324b0 EAX/EDX/EBX/ECX */

uint32_t fseek_f2183(ef_addr32 stream, uint32_t offset, uint32_t whence);

void draw_widget_a5b70(ef_addr32 w);

uint32_t expand_placeholders_a80b0(ef_addr32 text);

void send_packets_3617c(uint32_t total, uint32_t flag);

uint32_t init_sound_driver_d6e58(void);

uint32_t get_realmode_vector_111118(uint32_t intno);
void register_locked_ranges_117ace(void);

uint32_t close_handle_fa9a0(uint32_t handle);
void set_timer_divisor_110f0c(uint32_t divisor);

void send_serial_text_e10a0(ef_addr32 text);

void init_serial_port_e0930(void);
ef_addr32 localtime_1100d2(ef_addr32 timep, ef_addr32 tm);
uint32_t guest_fn_10e97f_ret_x(ef_addr32 tm); /* 0x10e97f EAX -> EAX */

void set_serial_divisor_e06ac(void);
void push_ring_record_8fbc8(ef_addr32 src);

void dump_list_b2d04(ef_addr32 list, uint32_t e, uint32_t file, ef_addr32 fmt, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);
uint32_t guest_fn_bc5d0_x(ef_addr32 obj); /* 0xbc5d0 EAX -> AX */
void guest_fn_b2b00_x(ef_addr32 node, uint32_t b, uint32_t c); /* 0xb2b00 EAX/EDX/EBX */

void rescale_dialog_widgets_35de0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double scale);







void finish_trace_c594(void);
void guest_fn_b3084_x(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5, uint32_t s6); /* 0xb3084 EAX/EDX/EBX/ECX, ret 0x1c */

void rebuild_angle_record_942cc(void);

void rebuild_angle_record_rev_94370(void);

uint32_t within_range_16b0c(uint32_t a, uint32_t b);



void try_engagement_168d8(uint32_t a, uint32_t b, uint32_t skip);
void guest_fn_16c7c_x(uint32_t record); /* 0x16c7c EAX */
void guest_fn_16e60_x(uint32_t a, uint32_t b); /* 0x16e60 EAX/EDX */


void guest_fn_16c7c_x(uint32_t record); /* 0x16c7c EAX */
void guest_fn_16e60_x(uint32_t a, uint32_t b); /* 0x16e60 EAX/EDX */

void clear_missile_slots_16c7c(uint32_t a);
void guest_fn_56f0c_x(int32_t unit, uint32_t index); /* 0x56f0c EAX/EDX */

void update_target_2631c(uint32_t flag);
void guest_fn_73084_x(void); /* 0x73084 */
void guest_fn_2479c_x(int32_t a, int32_t b, uint32_t c); /* 0x2479c EAX/EDX/EBX */
void guest_fn_24f94_x(void); /* 0x24f94 */
double guest_fn_6a5f0_x(int32_t a, int32_t b); /* 0x6a5f0 EAX/EDX -> ST0 */
void guest_fn_2541c_x(int32_t a, uint32_t b); /* 0x2541c EAX/EDX */
void guest_fn_2611c_x(int32_t a); /* 0x2611c EAX */
void guest_fn_24f24_x(void); /* 0x24f24 */
void guest_fn_24c40_x(int32_t a); /* 0x24c40 EAX */

void draw_circle_scaled_c9680(uint32_t colour, uint32_t d, uint32_t b, uint32_t r);

void report_bad_record_d68(ef_addr32 p);
uint32_t guest_fn_d08_x(uint32_t a); /* 0xd08 EAX -> EAX */
void guest_callptr26_x(ef_addr32 fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8, uint32_t a9, uint32_t a10, uint32_t a11, uint32_t a12, uint32_t a13, uint32_t a14, uint32_t a15, uint32_t a16, uint32_t a17, uint32_t a18, uint32_t a19, uint32_t a20, uint32_t a21, uint32_t a22, uint32_t a23, uint32_t a24, uint32_t a25); /* indirect cdecl call with 26 stack arguments, caller cleanup */



void parse_path_setting_e3f00(ef_addr32 text);



void pack_settings_record_35c08(void);

void follow_target_6be28(void);
uint32_t guest_fn_6ae30_x(int32_t a); /* 0x6ae30 EAX -> AL */
void guest_fn_2461c_x(int32_t a, ef_addr32 vec); /* 0x2461c EAX/EDX */

void tick_cycle_list_5414c(void);
void tick_cycle_list_54274(void);
void guest_fn_2e740_x(int32_t a, double x, uint32_t z); /* 0x2e740 EAX + stack double and dword, ret 0xc */
void guest_fn_37eac_x(int32_t a, double x, uint32_t z); /* 0x37eac EAX + stack double and dword, ret 0xc */

void create_unit_87db8(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5);
ef_addr32 guest_fn_879c4_x(void); /* 0x879c4 -> EAX */
void guest_fn_87a20_x(ef_addr32 obj, uint32_t a, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5, uint32_t s6); /* 0x87a20 EAX/EDX/EBX/ECX + 7 stack, ret 0x1c */
uint32_t guest_fn_80abc_x(void); /* 0x80abc -> AX */
void guest_fn_88100_x(ef_addr32 obj); /* 0x88100 EAX */

void pump_serial_transmit_117b18(ef_addr32 p);

uint32_t fold_list_kinds_c02ec(uint32_t a, uint32_t list, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);
uint32_t guest_fn_c03b4_x(int32_t cur, uint32_t kind, uint32_t j, uint32_t list, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5); /* 0xc03b4 EAX/EDX/EBX/ECX + 6 stack, ret 0x18, -> AX */

void check_data_files_284ec(void);
uint32_t guest_fn_27d18_x(ef_addr32 req); /* 0x27d18 EAX -> AX */
uint32_t guest_fn_28224_x(uint32_t mode); /* 0x28224 AL -> AX */

void load_directory_list_7a66c(ef_addr32 path);
int32_t guest_fn_df26f_x(ef_addr32 src, ef_addr32 fmt, ...); /* 0xdf26f cdecl, sscanf-like */

void jitter_offset_322ac(void);

void mirror_record_floats_1debc(ef_addr32 r);

uint32_t clip_segment_ce9a4(ef_addr32 r);
uint32_t guest_callptr4_ret_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* indirect call: EAX/EDX/EBX/ECX register arguments, result in EAX */
ef_addr32 guest_fn_ce5f8_x(ef_addr32 plane, ef_addr32 inside, ef_addr32 outside); /* 0xce5f8 EAX/EDX/EBX -> EAX */

#endif
