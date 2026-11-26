/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* The mission main loop: one pass per game frame until the mission ends. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define FRAME_PAUSED    EF_U8(0x00122c5du)  /* this frame stands still */
#define LOCAL_PAUSED    EF_U8(0x00122c5cu)
#define MISSION_DONE    EF_U16(0x00126518u)
#define FRAME_STEP      EF_U16(0x00127d92u) /* ticks per frame */
#define FRAME_STEP_HIGH EF_U32(0x00127d90u)
#define NETWORK_GAME    EF_U8(0x00127da9u)
#define SPLIT_SCREEN    EF_U8(0x00127da8u)
#define PLAYER_COUNT    EF_S16(0x001357b0u)
#define PLAYER_PAUSED(i) EF_U8(0x001a63a8u + (uint32_t)(i))
#define LOCAL_PLAYER    EF_U8(0x001a666bu)
#define TWO_SEATS       EF_U8(0x001a8580u)
#define SECOND_STICK    EF_U8(0x001a8581u)
#define CONTROL_DELAY   EF_S16(0x001a35ecu)
#define SUBSTEP_CLOCK   EF_S16(0x001a35e6u)
#define SUBSTEP_DUE     EF_U8(0x00137a00u)
#define SELECTED_UNIT   EF_S16(0x001f71cau) /* 0xff = none */
#define VIEW_MODE       EF_U8(0x00196fe8u)
#define DEBUG_PAGE      EF_U8(0x00122c52u)
#define END_TIMER       EF_U32(0x002143e8u)
#define SCORE_TEXT_LOCK EF_U8(0x00127b1au)

/* Per-player score words (team flag 0x12584c[i]). */
#define TEAM(i)         EF_U8(0x0012584cu + (uint32_t)(i))
#define SCORE_KILLS(i)  EF_U16(0x001a6388u + (uint32_t)(i) * 2u)
#define SCORE_LOSSES(i) EF_U16(0x001a6378u + (uint32_t)(i) * 2u)
#define SCORE_BONUS(i)  EF_U16(0x001a6398u + (uint32_t)(i) * 2u)
#define SCORE_RULE      EF_U8(0x0012584bu)  /* 1 = per player, 3 = losses count twice */
#define TEAM_A_SCORE    EF_U16(0x001a6638u)
#define TEAM_B_SCORE    EF_U16(0x001a663au)

#define SCREEN_WIDTH    EF_U32(0x0015b744u)
#define SCREEN_HEIGHT   EF_U32(0x0015b748u)

/* Self-modified immediate operands of the renderer at 0xc4754..0xc4766. */
#define EYE_OPERAND(i)  EF_U32(0x000c4756u + (uint32_t)(i) * 6u)
#define EYE_HIGH(i)     EF_U16(0x00159094u + (uint32_t)(i) * 4u)

#define WAIT_TEXT       0x00183964u

void guest_fn_52f38_x(void);
void guest_fn_90c6c_x(void);
uint8_t guest_fn_7e8c0_x(uint32_t peer);
void guest_fn_1cd8_x(void);
void guest_fn_76704_x(void);
void guest_fn_3d4b0_x(void);
void guest_fn_3f7fc_x(void);
void guest_fn_3f370_x(void);
void guest_fn_3efa8_x(void);
void guest_fn_3d37c_x(void);
void guest_fn_48e7c_x(void);
void guest_fn_5795c_x(uint8_t paused);
void guest_fn_11820_x(void);

static int16_t high_word(uint32_t value)
{
	return (int16_t)(value >> 16);
}

/* Width-aligned score line at the top right, x = 300 - width. */
static int16_t right_aligned(ef_addr32 text)
{
	return (int16_t)(0x12c - formatted_width_48d5c(text));
}

static void draw_scores(void)
{
	char text[0x20];
	ef_addr32 buffer = (ef_addr32)(uintptr_t)text;
	uint16_t team_a;
	uint16_t team_b;
	uint16_t score;
	int16_t i;

	latch_values_ce328(0, SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT - 1);
	store_four_dwords_17cf9c(0, SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT - 1);
	if (SCORE_RULE == 1) {
		if (SELECTED_UNIT <= 8) {
			guest_sprintf_a4342(buffer, 0x0012fdacu, (int32_t)(int16_t)SCORE_KILLS(LOCAL_PLAYER));
			SCORE_TEXT_LOCK = 1;
			draw_shadowed_text_48c90(0xf, 10, 10, buffer);
		}
		if (SELECTED_UNIT <= 8) {
			guest_sprintf_a4342(buffer, 0x0012fdb9u, (int32_t)(int16_t)SCORE_LOSSES(LOCAL_PLAYER));
			SCORE_TEXT_LOCK = 1;
			draw_shadowed_text_48c90(0xf, right_aligned(buffer), 10, buffer);
		}
		return;
	}
	team_a = 0;
	team_b = 0;
	for (i = 0; i < PLAYER_COUNT; i++) {
		score = (uint16_t)(SCORE_KILLS(i) * 2u);
		if (SCORE_RULE == 3)
			score -= (uint16_t)(SCORE_LOSSES(i) * 2u);
		else
			score -= SCORE_LOSSES(i);
		score += SCORE_BONUS(i);
		if (TEAM(i) == 0)
			team_a += score;
		else
			team_b += score;
	}
	TEAM_B_SCORE = team_b;
	TEAM_A_SCORE = team_a;
	if (SELECTED_UNIT <= 8) {
		guest_sprintf_a4342(buffer, 0x00132196u, (int32_t)(int16_t)team_a);
		SCORE_TEXT_LOCK = 1;
		if (TEAM(LOCAL_PLAYER) == 0)
			draw_shadowed_text_48c90(2, 10, 10, buffer);
		else
			draw_shadowed_text_48c90(2, right_aligned(buffer), 10, buffer);
	}
	if (SELECTED_UNIT <= 8) {
		guest_sprintf_a4342(buffer, 0x001321a7u, (int32_t)(int16_t)TEAM_B_SCORE);
		SCORE_TEXT_LOCK = 1;
		if (TEAM(LOCAL_PLAYER) != 0)
			draw_shadowed_text_48c90(0xc, 10, 10, buffer);
		else
			draw_shadowed_text_48c90(0xc, right_aligned(buffer), 10, buffer);
	}
}

/* Network game: when every player's object has landed (flag 8, or flag 4
 * with a zero speed word), record the track and announce readiness. */
static void check_all_landed(void)
{
	ef_addr32 object;
	int16_t landed = 0;
	int16_t i;
	uint8_t flags;

	for (i = 0; i < PLAYER_COUNT; i++) {
		object = lookup_filtered_indexed_object(
			high_word(EF_U32(0x001a5c2eu + (uint32_t)(int32_t)i * 0x1bu)));
		if (object == 0)
			continue;
		flags = EF_U8(object + 0x24u);
		if ((flags & 8) != 0 || ((flags & 4) != 0 && EF_U16(object + 0x22u) == 0))
			landed++;
	}
	if (landed == PLAYER_COUNT) {
		record_track_sample_2e494();
		announce_ready_56bdc();
	}
}

static void draw_debug_page(void)
{
	switch (DEBUG_PAGE) {
	case 1: guest_fn_3d4b0_x(); break;
	case 2: guest_fn_3f7fc_x(); break;
	case 3: refresh_selected_unit_40428(); break;
	case 4: guest_fn_3f370_x(); break;
	case 5: guest_fn_3efa8_x(); break;
	case 6: draw_debug_overlay_58714(); break;
	case 7: draw_ratio_text_40624(); break;
	case 8: draw_grid_3d904(); break;
	case 10: guest_fn_3d37c_x(); break;
	case 11: draw_debug_counters_404bc(); break;
	}
}

/* Simulate, draw and present one running frame. */
static void run_frame(uint8_t *last_sent)
{
	ef_addr32 message;
	uint32_t slot;

	EF_U8(0x001a3645u) = 0;
	if (VIEW_MODE == 1)
		guest_fn_1cd8_x();
	scene_view_dispatch_95e58();
	advance_camera_chain_97b18();
	if (!FRAME_PAUSED)
		draw_chain_97464();
	store_runtime_control_word(5);
	if (EF_U16(0x00425bc8u) != 0 && high_word(EF_U32(0x00425bceu)) >= EF_S32(0x00214448u)) {
		set_global_39310(2);
		draw_tile_window_7ac74(high_word(EF_U32(0x00425bc0u)), EF_S32(0x0021444cu),
			EF_U32(0x00214448u), EF_S32(0x00214450u),
			(uint32_t)((int32_t)high_word(EF_U32(0x00425ba8u)) + high_word(EF_U32(0x00425bceu))));
	} else {
		if (EF_U16(0x00425bc8u) != 0)
			set_global_39310(0);
		draw_tile_window_7ac74(high_word(EF_U32(0x001a35f8u)), EF_S32(0x0021444cu),
			EF_U32(0x00214448u), EF_S32(0x00214450u), 0);
	}
	draw_nearby_entries_87fe4();
	draw_effects_at_camera_8bb18();
	update_effect_entries_90f50();
	draw_markers_2f740();
	age_effect_nodes_288c8();
	play_queued_effects_1d474();
	update_grid_nodes_2a7f8();
	fade_effect_slots_8fc24();
	draw_effect_records_80c3c();
	store_runtime_control_word(6);
	if (EF_U16(0x001a35f2u) != 0)
		EF_U16(0x001a35f2u) = read_saved_tables_e34e8();
	visit_all_scene_nodes();
	store_runtime_control_word(7);
	if (step_view_transition_913dc(0xff, 2) < 4) {
		update_view_target_817a8();
		pop_triple_2a970();
		adjust_zoom_7baa4();
		renderer_finish_queue_c4167();
		if (step_view_transition_913dc(0xff, 2) == 3)
			call_e9a24_c3850();
	}
	store_runtime_control_word(8);
	store_runtime_control_word(9);
	if (!FRAME_PAUSED) {
		EF_U32(0x001228a4u) += FRAME_STEP;
		if (LOCAL_PLAYER == 0) {
			advance_clock_ticks_407c4(0x0012583cu, high_word(FRAME_STEP_HIGH));
			if (NETWORK_GAME && *last_sent != EF_U16(0x00125840u)) {
				send_triple_56f6c();
				*last_sent = EF_U8(0x00125840u);
			}
		}
	}
	store_runtime_control_word(0xa);
	if (VIEW_MODE == 1)
		draw_tactical_header_3b58();
	slot = (uint32_t)EF_U8(0x0012db9eu) * 0x1cu;
	if (EF_U32(0x00125ad4u + slot) != 0 && EF_U8(0x001282c4u) == 0)
		message = EF_U32(0x00125ad4u + slot);
	else
		message = EF_U32(0x00125ad0u + (uint32_t)EF_U8(0x0012db9eu) * 0x1cu);
	draw_pending_message_912b8(message);
	store_runtime_control_word(0xc);
	store_runtime_control_word(0xd);
	guest_fn_76704_x();
	store_runtime_control_word(0xe);
	run_timed_events_3cf08();
	if (EF_U32(0x001a35acu) != 0xffffffffu)
		tick_timer_3ce30();
	if ((EF_U8(0x00425bc4u) & 8) != 0)
		nop_81864();
	EF_U32(0x00127dc0u) = find_tagged_node_3cd6c();
	store_runtime_control_word(0xf);
	EYE_OPERAND(0) = EF_U32(0x002143f4u);
	EYE_OPERAND(1) = EF_U32(0x002143f0u);
	EYE_OPERAND(2) = EF_U32(0x002143ecu);
	EYE_HIGH(0) = (uint16_t)high_word(EYE_OPERAND(0));
	EYE_HIGH(1) = (uint16_t)high_word(EYE_OPERAND(1));
	EYE_HIGH(2) = (uint16_t)high_word(EYE_OPERAND(2));
	set_params_and_run_c4654(0, 0, 0, 0, (uint32_t)(int32_t)high_word(EF_U32(0x002144a6u)),
		(uint32_t)(int32_t)high_word(EF_U32(0x002144a4u)),
		(uint32_t)(int32_t)high_word(EF_U32(0x002144a2u)));
	draw_debug_page();
	if (SPLIT_SCREEN)
		draw_scores();
	if (NETWORK_GAME)
		tick_broadcast_ring_56318();
	update_timers_48064();
	guest_fn_48e7c_x();
	update_hud_79128();
	if (EF_U16(0x001a35f2u) != 0) {
		draw_text_e30e0(0, 0x137, 1, EF_U32(0x00126544u));
		draw_text_e30e0(6, 0x136, 0, EF_U32(0x00126544u));
	}
	draw_status_counter_48df8();
	guest_fn_5795c_x(FRAME_PAUSED);
	guest_fn_11820_x();
	if (NETWORK_GAME && !SPLIT_SCREEN && LOCAL_PLAYER == 0)
		check_all_landed();
}

/* 0x3dd30, 2924 bytes. Reset the mission state, then run frames until
 * MISSION_DONE (word 0x126518) is set by a callee or by the end timer
 * 0x2143e8 running out. Each frame first works out FRAME_PAUSED (the
 * local pause, or in a network game any player's pause byte), switching
 * the UI releases on and off with it; a running frame advances the
 * sub-step clock and the simulation, then simulates, draws and presents
 * the frame. A waiting frame (no unit selected and no stage word, the
 * hold flags, or a network peer behind) only clears the screen and
 * shows the wait text. On exit a finished local single-player mission
 * replays its elapsed steps. */
void mission_loop_3dd30(void)
{
	uint32_t releases = 0;
	uint8_t last_sent = 0;
	int16_t i;

	dos_commit_f44();
	FRAME_PAUSED = 0;
	reseed_generator_a45a8();
	reset_selection_7597c();
	EF_U32(0x00132ae0u) = 0x0001d470u;
	refresh_node_progress_1e28c();
	reset_handler_1d420();
	MISSION_DONE = 0;
	if (get_flag_7eebc() != 0)
		start_page_7feb0();
	set_control_mode_a(0);
	set_control_mode_b(0);
	if (TWO_SEATS) {
		set_control_mode_b(1);
		set_control_mode_c(0);
	}
	set_runtime_phase_five();
	guest_fn_52f38_x();
	clear_tactical_accumulators();
	clear_runtime_transient_counters();
	reset_slots_77be0();
	reset_tables_45ba0();
	FRAME_STEP = 5;
	advance_runtime_tick();
	EF_U8(0x0041f833u) = 0;
	LOCAL_PAUSED = 0;
	EF_U8(0x001a3623u) = 0;
	reset_selection_540d0();
	init_sequence_5f60();
	init_chain_97a4c();
	store_pending_name_57714();
	init_menu_entries_8fa84();
	reset_channels_3dccc();
	reseed_generator_a45a8();
	if (NETWORK_GAME)
		clear_dwords_135820();
	if (EF_U8(0x00137204u))
		build_shade_tables_d6478();

	do {
		FRAME_PAUSED = LOCAL_PAUSED;
		if (NETWORK_GAME) {
			update_frame_delta_a4138();
			for (i = 0; i < PLAYER_COUNT; i++)
				FRAME_PAUSED |= PLAYER_PAUSED(i);
			if (FRAME_PAUSED) {
				ui_initialize_releases();
				releases = 1;
			} else if ((uint16_t)releases != 0) {
				ui_finalize_releases();
				releases = 0;
			}
		} else if (FRAME_PAUSED) {
			ui_initialize_releases();
		}
		if (!FRAME_PAUSED) {
			run_entry_chain_a0060();
			update_tick_rate_e3120();
		}
		if (CONTROL_DELAY != 0 && --CONTROL_DELAY == 0) {
			if (SECOND_STICK)
				set_control_mode_a(1);
			if (TWO_SEATS) {
				set_control_mode_c(1);
				set_control_mode_b(1);
			}
		}
		if (!FRAME_PAUSED) {
			SUBSTEP_CLOCK += FRAME_STEP;
			if (SUBSTEP_CLOCK > 7) {
				SUBSTEP_CLOCK -= 7;
				SUBSTEP_DUE = 1;
				if (SUBSTEP_CLOCK > 100)
					SUBSTEP_CLOCK = 0;
			} else {
				SUBSTEP_DUE = 0;
			}
			refresh_frame_tables_54110();
			run_timed_events_3db9c();
			scan_triggers_3d950();
			check_unit_leaves_map_3dbf0();
			age_timers_1e684();
			check_base_contact_9114();
			guest_fn_7ef14_x();
			forward_sound_state_982d4();
			update_sound_mode_98198();
		}
		store_runtime_control_word(2);
		simulation_frame_1cde8();
		store_runtime_control_word(3);
		if (!FRAME_PAUSED) {
			refresh_selection_frame_8bb50();
			guest_fn_90c6c_x();
		}
		store_runtime_control_word(4);
		if (!FRAME_PAUSED)
			run_reset_sequence_3dcb0();
		setup_viewport_window_76620();
		reset_view_c4105();
		latch_values_ce328(0, SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT - 1);
		update_view_scale_dc314();
		if (EF_U8(0x0012b369u))
			set_view_shift_dc334(0, 0, 0, 0, 1, 1);
		store_runtime_control_word(0x28);
		if (NETWORK_GAME) {
			get_net_data_581b0();
			tail_jump_58094(0);
		}
		if (EF_U8(0x001264e1u)) {
			if (guest_fn_7e8c0_x(EF_U32(0x0012b06cu)) == 0) {
				announce_ready_57030();
				tail_jump_58094(0);
			}
			EF_U8(0x001264e1u) = 0;
		}
		if ((SELECTED_UNIT == 0xff && EF_U16(0x00126516u) == 0) || EF_U8(0x0019bc02u)
				|| EF_U8(0x001f9010u)
				|| (LOCAL_PLAYER == 0 && EF_U8(0x00127da4u) < EF_U8(0x0012583au))) {
			clear_buffer_dc2c0();
			if (NETWORK_GAME)
				draw_shadowed_text_48c90(0xf,
					(int16_t)(0xa0 - ((int32_t)(int16_t)formatted_width_48d5c(WAIT_TEXT) >> 1)),
					0x64, WAIT_TEXT);
			update_frame_time_53270();
		} else {
			run_frame(&last_sent);
		}
		cycle_page_dc280();
		process_ring_880c8();
		if (EF_U8(0x0012dbc4u))
			advance_view_queue_91c98();
		if (END_TIMER != 0) {
			END_TIMER -= FRAME_STEP;
			if ((int32_t)END_TIMER < 0)
				MISSION_DONE = 1;
		}
	} while (MISSION_DONE == 0);

	VIEW_MODE = 0;
	if (EF_U8(0x00125846u) == 3 && LOCAL_PLAYER == 0) {
		call_79ff0_12af60();
		replay_elapsed_steps_c630();
		reset_menu_80120();
	}
	reroll_list_entries_1e3c4();
}
