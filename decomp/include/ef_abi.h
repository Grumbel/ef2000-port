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

#endif
