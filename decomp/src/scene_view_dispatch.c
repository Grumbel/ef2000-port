/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Per-frame camera dispatcher: one step of the view selected by
 * g_scene_control_code. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define VIEW_MATRIX   ((ef_addr32)(uintptr_t)g_scene_view_matrix)
#define CAMERA_MATRIX ((ef_addr32)(uintptr_t)g_scene_camera_matrix)

/* Nine-dword orientation matrix of the player aircraft. */
#define PLAYER_ROTATION 0x00433f08u
#define ROTATION(i)     EF_S32(PLAYER_ROTATION + (uint32_t)(i) * 4u)

/* Low word: replay flag; high word: one-shot stage (1 -> reset view). */
#define REPLAY_FLAG     EF_U16(0x00122c4cu)
#define REPLAY_STAGE    EF_U16(0x00122c4eu)

#define PLAYER_ANGLE(i) EF_U16(0x00127d3cu + (uint32_t)(i) * 2u)
#define SCENE_ANGLE_WORD EF_U16(0x0012db9au)
#define MODE6_TIMER     EF_U16(0x0012dcb6u)
#define FALLBACK_UNIT   EF_U8(0x0012dc34u)
#define LOCK_SUBJECT_A  EF_U8(0x001264e0u)
#define LOCK_SUBJECT_B  EF_U8(0x00196fe8u)

/* Key-view (0x5/0x6) and cockpit (0x0..0x8, 0x49) state. */
#define KEY_VIEW_ACTIVE EF_U8(0x0019e5dfu)
#define KEY_VIEW_KIND   EF_U8(0x0019e555u)
#define KEY_VIEW_FLAG   EF_U8(0x0019e556u)
#define KEY_VIEW_OBJECT EF_U32(0x0019e54du)
#define KEY_VIEW_UNIT   EF_U32(0x0019e551u)
#define KEY_VIEW_DONE   EF_U8(0x0019e5e1u)
#define KEY_VIEW_X      EF_U32(0x0019e57eu)
#define KEY_VIEW_Y      EF_U32(0x0019e582u)

#define TRACKED_UNIT    EF_U32(0x001a8234u) /* signed high word: unit id, 0xff = none */
#define TRACKED_OBJECT  EF_U32(0x001a821cu)
#define TRACKED_OTHER   EF_U32(0x001a8210u)
#define FOLLOW_TARGET   EF_U32(0x0020f15cu)
#define FOLLOW_ENABLED  EF_U32(0x0020f154u)

#define FOLLOW_DISTANCE EF_U32(0x0021433cu)
#define FOLLOW_ROLL     EF_U16(0x00214468u)
#define LAST_PLAYER_X   EF_U16(0x00214554u) /* stored as dwords, compared as words */
#define LAST_PLAYER_H   EF_U32(0x00214558u)
#define LAST_PLAYER_Z   EF_U16(0x0021455cu)
#define RETURN_MODE     EF_U8(0x00214486u)
#define VIEW_TRANSITION EF_U8(0x00214547u)
#define ANGLES_LOCKED   EF_U8(0x00214549u)
#define SUBJECT_LOCKED  EF_U8(0x0021454au)

#define NO_UNIT 0xff

static int16_t high_word(uint32_t value)
{
	return (int16_t)(value >> 16);
}

static int16_t subject_field_id(ef_addr32 object, uint32_t offset)
{
	return high_word(EF_U32(object + offset));
}

static void clear_view_offset(void)
{
	g_view_offset[2] = 0;
	g_view_offset[1] = 0;
	g_view_offset[0] = 0;
}

static void copy_subject_position(void)
{
	int i;

	for (i = 0; i < 3; ++i)
		g_scene_camera_position[i] = EF_S32(g_scene_subject + (uint32_t)i * 4u);
}

/* Use the subject's view-data word when it has one. */
static void take_subject_view_word(void)
{
	if (scene_lookup_named_word((int32_t)EF_U32(g_scene_subject + 0x1eu) >> 16) != 0)
		g_view_data_word = (uint16_t)scene_lookup_named_word((int32_t)EF_U32(g_scene_subject + 0x1eu) >> 16);
}

/* Fall back to the view stored in RETURN_MODE and dispatch again. */
static void return_to_saved_mode(void)
{
	g_scene_control_code = RETURN_MODE;
	g_scene_change_pending = 1;
}

static void request_mode(uint8_t mode)
{
	g_scene_control_code = mode;
	g_scene_change_pending = 1;
}

/* Cockpit views: hand the frame to 0x3122c. */
static void cockpit_view(void)
{
	SUBJECT_LOCKED = 1;
	KEY_VIEW_ACTIVE = 1;
	guest_fn_3122c();
	request_scene_refresh();
}

/* Camera matrix from the follow target's angles (pitch offset by a quarter
 * turn back, heading by a quarter turn forward). */
static void aim_camera_at_follow_target(void)
{
	init_bounds_d0b68(CAMERA_MATRIX);
	update_scene_angle_first(CAMERA_MATRIX, (uint16_t)(EF_U16(FOLLOW_TARGET + 0xeu) - 0x100u));
	update_scene_angle_second(CAMERA_MATRIX, (uint16_t)(EF_U16(FOLLOW_TARGET + 0xcu) + 0x100u));
	update_scene_angle_third(CAMERA_MATRIX, EF_U16(FOLLOW_TARGET + 0x10u));
}

static void start_follow_camera(ef_addr32 target)
{
	g_scene_follow_object = EF_U32(target + 0x30u);
	g_scene_follow_angles[0] = 0x2000u;
	g_scene_start_position[0] = EF_S32(target);
	FOLLOW_DISTANCE = 0xa00u;
	g_scene_start_position[1] = EF_S32(target + 4u);
	g_scene_start_position[2] = EF_S32(target + 8u);
	g_scene_intro_timer = 0xc8;
	g_scene_follow_angles[1] = 0u;
	FOLLOW_ROLL = 0u;
}

static void place_camera_on_follow_target(ef_addr32 target)
{
	int i;

	clear_view_offset();
	for (i = 0; i < 3; ++i)
		g_scene_camera_position[i] = EF_S32(target + (uint32_t)i * 4u);
	aim_camera_at_follow_target();
	update_chase_camera_9357c();
}

/* Missing follow target: reset the view matrix and leave in view 0x3d. */
static void abandon_follow_camera(void)
{
	g_scene_update_latch = 0x14u;
	init_bounds_d0b68(VIEW_MATRIX);
	g_scene_change_pending = 1;
	g_scene_control_code = 0x3d;
}

/* Views 0x37..0x3a, 0x41: subject-aligned camera plus a named scene entry. */
static void subject_entry_view(ef_addr32 entry, int clear_offset)
{
	g_scene_event_armed = 1;
	init_bounds_d0b68(CAMERA_MATRIX);
	update_scene_angle_first(CAMERA_MATRIX, EF_U16(g_scene_subject + 0xeu));
	update_scene_angle_second(CAMERA_MATRIX, EF_U16(g_scene_subject + 0xcu));
	update_scene_angle_third(CAMERA_MATRIX, EF_U16(g_scene_subject + 0x10u));
	lookup_entry_95660(entry);
	request_scene_refresh();
	if (clear_offset)
		clear_view_offset();
}

static void tracked_camera_view(void)
{
	ANGLES_LOCKED = 1;
	g_view_offset[2] = 0;
	update_tracked_camera_94a74();
	request_scene_refresh();
}

/* 0x0c: camera on the player using the player's own angles. */
static void player_angle_view(void)
{
	ef_addr32 subject;

	request_scene_pair((int16_t)(-PLAYER_ANGLE(0)), (int16_t)(-PLAYER_ANGLE(2)));
	shift_triples_to_1590c0_c4680(PLAYER_ROTATION);
	configure_scene_angle_triplet((int16_t)PLAYER_ANGLE(0), (int16_t)PLAYER_ANGLE(1),
	                              (int16_t)PLAYER_ANGLE(2));
	subject = g_scene_subject;
	set_scene_input_triplet((int32_t)(EF_U32(subject) << 8), (int32_t)(EF_U32(subject + 4u) << 8),
	                        (int32_t)(EF_U32(subject + 8u) << 8));
	g_scene_query_pair[0] = (ef_addr32)(EF_S32(subject) >> 8);
	g_scene_gate_limit = EF_S32(subject + 4u) >> 8;
	g_scene_query_pair[1] = (ef_addr32)(EF_S32(subject + 8u) >> 8);
	select_page_layout_9dfc0(subject_field_id(subject, 0x1eu), (uint16_t)(EF_S32(subject) >> 8),
	                         (uint16_t)(EF_S32(subject + 4u) >> 8), (uint16_t)(EF_S32(subject + 8u) >> 8));
	request_scene_refresh();
}

/* 0x10: fly-by camera at the follow target, held above the ground. */
static void flyby_view(void)
{
	int32_t eye[3];
	ef_addr32 target;

	set_view_shift_dc334(0u, 0u, 0u, 0u, 0xffffffffu, 0xffffffffu);
	request_scene_refresh();
	target = FOLLOW_TARGET;
	if (target == 0u) {
		request_mode(0x0f);
		return;
	}
	g_scene_query_pair[0] = (ef_addr32)((EF_S32(target) >> 8) + (g_view_motion[0] >> 16));
	g_scene_gate_limit = (EF_S32(target + 4u) >> 8) + (g_view_motion[1] >> 16);
	g_scene_query_pair[1] = (ef_addr32)((EF_S32(target + 8u) >> 8) + (g_view_motion[2] >> 16));
	eye[0] = EF_S32(target);
	eye[1] = EF_S32(target + 4u);
	eye[2] = EF_S32(target + 8u);
	advance_position_cef3c((ef_addr32)(uintptr_t)eye, target + 0xcu, EF_U32(target + 0x26u) * 0x32u);
	if (scene_metric_reaches_threshold((int32_t)g_scene_query_pair[0], g_scene_gate_limit) == 0u) {
		g_scene_gate_limit = scene_metric_remaining();
		eye[1] = (int32_t)((uint32_t)g_scene_gate_limit << 8);
	}
	set_scene_input_triplet((int32_t)((uint32_t)eye[0] << 8), (int32_t)((uint32_t)eye[1] << 8),
	                        (int32_t)((uint32_t)eye[2] << 8));
	target = FOLLOW_TARGET;
	set_scene_angles((int16_t)(-EF_U16(target + 0xcu)), (int16_t)(-EF_U16(target + 0xeu)),
	                 (int16_t)(-EF_U16(target + 0x10u)));
	target = FOLLOW_TARGET;
	guest_fn_d86e0_x((uint16_t)(-EF_U16(target + 0xcu)), (uint16_t)(-EF_U16(target + 0x10u)));
}

/* 0x06: key view countdown; when it runs out switch to key view 0x05. */
static void key_view_countdown(void)
{
	ef_addr32 target;

	KEY_VIEW_ACTIVE = 1;
	target = FOLLOW_TARGET;
	if (target != 0u) {
		MODE6_TIMER = 0xc8u;
		KEY_VIEW_OBJECT = target;
		KEY_VIEW_KIND = 5;
	} else {
		uint16_t timer = (uint16_t)(MODE6_TIMER - g_simulation_tick_step);

		KEY_VIEW_DONE = 0;
		KEY_VIEW_X += 0xfau;
		MODE6_TIMER = timer;
		KEY_VIEW_Y -= 0xfau;
		if ((int16_t)timer < 0) {
			MODE6_TIMER = 0u;
			KEY_VIEW_DONE = 1;
			KEY_VIEW_KIND = 1;
			KEY_VIEW_OBJECT = lookup_filtered_indexed_object(high_word(g_grid_secondary_selector));
			select_key_view_f8_32b90();
			g_scene_control_code = 5;
		}
	}
	SUBJECT_LOCKED = 1;
	guest_fn_3122c();
	request_scene_refresh();
}

/* 0x3e: wait for a position bin; refresh it when its grid cell is clear. */
static void position_bin_view(void)
{
	int16_t unit = high_word(g_position_bin_packed);

	if (unit != NO_UNIT) {
		ef_addr32 object = lookup_filtered_indexed_object(unit);

		if (object != 0u) {
			int32_t cell = (int32_t)subject_field_id(object, 0x26u) * 0xca;

			if (EF_U8(g_world_grid_base + (uint32_t)cell + 0x20u) == 0u ||
			    (EF_U8(object + 0x24u) & 0x08u) != 0u)
				update_position_bin();
		}
	}
	unit = high_word(g_position_bin_packed);
	if (unit != NO_UNIT)
		load_camera_92e30((uint32_t)(int32_t)unit);
	else
		request_mode(0x40);
	request_scene_refresh();
}

/* 0x3f: navigation view on the position-bin unit, else 0x1b4a4. */
static void position_bin_navigation_view(void)
{
	int16_t unit = high_word(g_position_bin_packed);
	int navigate = 0;

	if (unit != NO_UNIT) {
		ef_addr32 object = lookup_filtered_indexed_object(unit);

		if (object != 0u) {
			int32_t cell = (int32_t)subject_field_id(object, 0x26u) * 0xca;

			navigate = EF_U8(g_world_grid_base + (uint32_t)cell + 0x20u) != 0u &&
			           (EF_U8(object + 0x24u) & 0x08u) == 0u &&
			           EF_U16(object + 0x146u) != NO_UNIT;
		}
	}
	if (navigate)
		update_navigation_target_94954(high_word(g_position_bin_packed));
	else
		guest_fn_1b4a4();
	SUBJECT_LOCKED = 1;
	request_scene_refresh();
}

/* 0x2e: navigation view on the tracked unit when it has a live target. */
static void tracked_navigation_view(void)
{
	int16_t unit = high_word(TRACKED_UNIT);
	ef_addr32 object;

	if (unit == NO_UNIT)
		goto fallback;
	object = lookup_filtered_indexed_object(high_word(TRACKED_UNIT));
	if (object == 0u || EF_U16(object + 0x146u) == NO_UNIT)
		goto fallback;
	if (lookup_filtered_indexed_object(subject_field_id(object, 0x144u)) == 0u)
		goto fallback;
	update_navigation_target_94954(high_word(TRACKED_UNIT));
	request_scene_refresh();
	return;
fallback:
	return_to_saved_mode();
	request_scene_refresh();
}

static void tracked_pair_view(void)
{
	if (high_word(TRACKED_UNIT) != NO_UNIT) {
		pair_with_target_948c4(high_word(TRACKED_UNIT));
	} else if (TRACKED_OBJECT != 0u) {
		update_view_94880(TRACKED_OBJECT);
	} else if (TRACKED_OTHER != 0u) {
		update_view_94880(TRACKED_OTHER);
	} else {
		return_to_saved_mode();
	}
	request_scene_refresh();
}

static void tracked_camera_load_view(void)
{
	if (high_word(TRACKED_UNIT) != NO_UNIT)
		load_camera_92e30((uint32_t)(int32_t)high_word(TRACKED_UNIT));
	else if (TRACKED_OBJECT != 0u)
		guest_fn_92c28(TRACKED_OBJECT);
	else if (TRACKED_OTHER != 0u)
		guest_fn_92c28(TRACKED_OTHER);
	else if (REPLAY_FLAG != 0u)
		guest_fn_92c28(0u);
	else
		return_to_saved_mode();
	request_scene_refresh();
}

/* Unit the subject's unit points at (+0x41), or FALLBACK_UNIT. */
static int16_t subject_partner_unit(ef_addr32 object)
{
	if (subject_field_id(object, 0x41u) != NO_UNIT)
		return subject_field_id(object, 0x41u);
	return (int16_t)FALLBACK_UNIT;
}

/* Dispatch one view; returns 0 when the frame ends without the tail. */
static int dispatch_view(void)
{
	ef_addr32 object;

	switch (g_scene_control_code) {
	case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x08: case 0x49:
		cockpit_view();
		break;
	case 0x05:
		KEY_VIEW_ACTIVE = 1;
		object = FOLLOW_TARGET;
		if (object != 0u) {
			KEY_VIEW_KIND = 5;
			KEY_VIEW_OBJECT = object;
			KEY_VIEW_FLAG = 1;
			KEY_VIEW_UNIT = lookup_filtered_indexed_object(high_word(g_grid_secondary_selector));
			call_32c04_182af8();
			g_scene_control_code = 6;
		}
		SUBJECT_LOCKED = 1;
		guest_fn_3122c();
		request_scene_refresh();
		break;
	case 0x06:
		key_view_countdown();
		break;
	case 0x30:
		ANGLES_LOCKED = 1;
		init_scene_angles_93e58();
		break;
	case 0x46:
		ANGLES_LOCKED = 1;
		reset_angle_limits_93f44();
		break;
	case 0x47:
		ANGLES_LOCKED = 1;
		init_scene_angles_94000();
		break;
	case 0x29:
		clear_view_offset();
		init_bounds_d0b68(VIEW_MATRIX);
		update_scene_angle_first(VIEW_MATRIX, 0x8000u);
		g_scene_event_armed = 1;
		g_scene_update_latch = 0u;
		SCENE_ANGLE_WORD = (uint16_t)forward_pair_d13dc(VIEW_MATRIX);
		guest_fn_93a8c();
		request_scene_refresh();
		break;
	case 0x20:
		update_scene_roll_941e4();
		break;
	case 0x1a:
		update_scene_camera_node_92984();
		request_scene_refresh();
		break;
	case 0x1b:
		update_camera_94484();
		break;
	case 0x27:
		rebuild_angle_record_942cc();
		break;
	case 0x26:
		rebuild_angle_record_rev_94370();
		break;
	case 0x12:
		ANGLES_LOCKED = 1;
		clear_view_offset();
		reset_scene_update_94194();
		break;
	case 0x3c:
		object = lookup_filtered_indexed_object(high_word(g_grid_secondary_selector));
		clear_view_offset();
		if (object != 0u)
			g_scene_subject = lookup_filtered_indexed_object(subject_field_id(object, 0x41u));
		reset_scene_update_94194();
		break;
	case 0x35:
		ANGLES_LOCKED = 1;
		clear_view_offset();
		reset_scene_update_941bc();
		break;
	case 0x0c:
		player_angle_view();
		break;
	case 0x23:
		select_scene_object();
		break;
	case 0x0d:
	case 0x2f:
		copy_subject_position();
		clear_view_offset();
		apply_rotated_offset_95bf4();
		if (g_scene_control_code == 0x0d)
			copy_record9_d0858(CAMERA_MATRIX, PLAYER_ROTATION);
		else
			init_bounds_d0b68(CAMERA_MATRIX);
		take_subject_view_word();
		update_chase_camera_9357c();
		request_scene_refresh();
		break;
	case 0x25: {
		uint16_t heading = (uint16_t)(PLAYER_ANGLE(1) + (uint16_t)(g_simulation_tick_step << 6));

		copy_subject_position();
		PLAYER_ANGLE(1) = heading;
		copy_record9_d0858(CAMERA_MATRIX, PLAYER_ROTATION);
		take_subject_view_word();
		update_chase_camera_9357c();
		request_scene_refresh();
		break;
	}
	case 0x44:
		object = FOLLOW_TARGET;
		if (object == 0u) {
			abandon_follow_camera();
			return 0;
		}
		start_follow_camera(object);
		if (EF_U32(object + 0x30u) != 0u)
			forward_record_field_30(object);
		else
			place_camera_on_follow_target(object);
		request_scene_refresh();
		break;
	case 0x43:
		object = FOLLOW_TARGET;
		if (object == 0u) {
			abandon_follow_camera();
			return 0;
		}
		start_follow_camera(object);
		place_camera_on_follow_target(object);
		request_scene_refresh();
		break;
	case 0x24:
		reset_view_97064();
		break;
	case 0x0f:
		setup_scene_camera_95ae0();
		break;
	case 0x3d:
		update_scene_intro_9596c();
		break;
	case 0x10:
		flyby_view();
		break;
	case 0x19:
		tracked_pair_view();
		break;
	case 0x2c:
	case 0x3b:
		object = lookup_filtered_indexed_object(high_word(g_grid_secondary_selector));
		if (object != 0u) {
			if (g_scene_control_code == 0x2c)
				look_from_player_94830(subject_partner_unit(object));
			else
				look_at_unit_947c4(subject_partner_unit(object));
			SUBJECT_LOCKED = 1;
		}
		request_scene_refresh();
		break;
	case 0x2e:
		tracked_navigation_view();
		break;
	case 0x48:
		if (high_word(TRACKED_UNIT) != NO_UNIT || TRACKED_OBJECT != 0u)
			look_at_target_94a1c(high_word(TRACKED_UNIT));
		else
			return_to_saved_mode();
		request_scene_refresh();
		break;
	case 0x3e:
		position_bin_view();
		break;
	case 0x3f:
		position_bin_navigation_view();
		break;
	case 0x40:
		if (high_word(g_position_bin_packed) != NO_UNIT) {
			pair_with_target_948c4(high_word(g_position_bin_packed));
			SUBJECT_LOCKED = 1;
			request_scene_refresh();
		} else {
			request_mode(0x3f);
		}
		break;
	case 0x2d:
		tracked_camera_load_view();
		break;
	case 0x2b:
		aim_scene_at_station_93060();
		request_scene_refresh();
		break;
	case 0x2a:
		g_scene_update_latch = 0x1f4u;
		latch_scene_camera_944f4();
		break;
	case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		tracked_camera_view();
		break;
	case 0x18:
		if (FOLLOW_ENABLED != 0u) {
			ANGLES_LOCKED = 1;
			g_view_offset[2] = 0;
			update_tracked_camera_94a74();
		} else {
			return_to_saved_mode();
		}
		request_scene_refresh();
		break;
	case 0x36:
		ANGLES_LOCKED = 1;
		g_view_offset[2] = 0;
		reset_scene_words_and_update();
		request_scene_refresh();
		break;
	case 0x28:
		guest_fn_95020();
		request_scene_refresh();
		break;
	case 0x31:
		reset_scene_record_92f10();
		request_scene_refresh();
		break;
	case 0x32:
		update_viewpoint_92f98();
		request_scene_refresh();
		break;
	case 0x34:
		guest_fn_93170();
		request_scene_refresh();
		break;
	case 0x33:
		update_chase_distance_9586c();
		request_scene_refresh();
		break;
	case 0x37:
		subject_entry_view(0x0018abdcu, 0);
		break;
	case 0x38:
		subject_entry_view(0x0018abf8u, 0);
		break;
	case 0x39:
		subject_entry_view(0x0018ac04u, 1);
		break;
	case 0x3a:
		subject_entry_view(0x0018ac10u, 1);
		break;
	case 0x41:
		subject_entry_view(0x0018ac1cu, 1);
		break;
	case 0x22: case 0x42: case 0x45:
		guest_fn_94d08();
		break;
	default:
		break;
	}
	return 1;
}

/* Feed the player's ground movement since the last frame, rotated into
 * the player's frame, to 0xd8358. */
static void track_player_motion(void)
{
	int32_t dz = (int16_t)(LAST_PLAYER_Z - (uint16_t)g_player_x);
	int32_t dx = (int16_t)(LAST_PLAYER_X - (uint16_t)g_player_y);
	int32_t z = (int32_t)((uint32_t)ROTATION(2) * (uint32_t)dz + (uint32_t)ROTATION(8) * (uint32_t)dx);
	int32_t y = (int32_t)((uint32_t)ROTATION(1) * (uint32_t)dz + (uint32_t)ROTATION(7) * (uint32_t)dx);
	int32_t x = (int32_t)((uint32_t)ROTATION(0) * (uint32_t)dz + (uint32_t)ROTATION(6) * (uint32_t)dx);
	uint32_t length = length_squared_word_triple((int16_t)(x >> 15), (int16_t)(y >> 15),
	                                             (int16_t)(z >> 15));

	EF_U32(0x0021455cu) = (uint32_t)g_player_x;
	LAST_PLAYER_H = (uint32_t)g_player_height;
	EF_U32(0x00214554u) = (uint32_t)g_player_y;
	advance_position_d8358((int16_t)length);
}

/* 0x95e58, 4618 bytes. One frame of the external camera. Looks up the
 * scene subject (falling back to view 0x3e when it is gone), then runs
 * the handler of view g_scene_control_code. A handler that switches view
 * sets g_scene_change_pending and the new view runs in the same frame;
 * after a second switch the loop forces view 0x35. Views 0x43/0x44
 * without a follow target return at once; otherwise the player's motion
 * is fed to 0xd8358 (except in views 0x0d and 0x32) and a pending view
 * transition is stepped. */
void scene_view_dispatch_95e58(void)
{
	int16_t redispatched = 0;
	uint8_t mode;

	if ((int16_t)REPLAY_STAGE == 1) {
		REPLAY_STAGE = 2;
		init_bounds_d0b68(VIEW_MATRIX);
		update_scene_angle_second(VIEW_MATRIX, 0x1000u);
	}
	g_scene_subject = lookup_filtered_indexed_object(high_word(g_grid_secondary_selector));
	if (g_scene_subject == 0u) {
		mode = g_scene_control_code;
		if (mode != 0x3e && mode != 0x09 && LOCK_SUBJECT_A == 0u && LOCK_SUBJECT_B == 0u)
			g_scene_control_code = 0x3e;
	}
	SUBJECT_LOCKED = 0;
	ANGLES_LOCKED = 0;
	g_view_data_word = REPLAY_FLAG != 0u ? 100u : 10u;
	VIEW_TRANSITION = 1;
	if (g_scene_control_code == 0x30)
		enter_mode_1a358(0u, 0u, 0u, 0u, 0x3e99999au); /* 0.3f */
	else
		leave_mode_1a390();

	g_scene_change_pending = 0;
	do {
		if (redispatched != 0)
			g_scene_control_code = 0x35;
		if (g_scene_change_pending != 0) {
			redispatched = 1;
			g_scene_change_pending = 0;
		}
		if (g_scene_control_code != 0x49)
			LOCK_SUBJECT_B = 0;
		if (!dispatch_view())
			return;
	} while (g_scene_change_pending != 0);

	mode = g_scene_control_code;
	if (mode != 0x0d && mode != 0x32)
		track_player_motion();
	if (VIEW_TRANSITION != 0u)
		step_view_transition_913dc(0xff, 4);
	g_scene_event_armed = 0;
}
