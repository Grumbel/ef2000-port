/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#define FRAME_DT          EF_F64(0x0019af70u)
#define HOVER_MIN_SPEED   EF_F64(0x001819b0u)
#define MAP_SCALE         EF_F64(0x001819b8u)
#define NEAR_DISTANCE     EF_F64(0x001819c0u)
#define WAYPOINT_Z_SCALE  EF_F64(0x001819c8u)
#define LOOKAHEAD         EF_F64(0x001819d0u)
#define CELL_SCALE        EF_F64(0x001819d8u)
#define TERRAIN_CLEARANCE EF_F64(0x001819e0u)
#define MIN_PITCH_ERROR   EF_F64(0x001819e8u)
#define PITCH_RATE_SCALE  EF_F64(0x001819f0u)

#define OBJ_KIND          0x41
#define OBJ_X             0x51
#define OBJ_Y             0x59
#define OBJ_Z             0x61
#define OBJ_PITCH_RATE    0x69
#define OBJ_SPEED         0x71
#define OBJ_HEADING       0x81
#define OBJ_PITCH         0x99
#define OBJ_WAYPOINTS     0xa1   /* pointer to 25-byte records */
#define OBJ_WAYPOINT_INDEX 0xa5  /* uint8 */

#define WAYPOINT_SIZE     25
#define WP_Y              0      /* int32 map units */
#define WP_X              4      /* int32 map units */
#define WP_Z              8      /* double */

enum { KIND_HOVER = 16 };

static double abs_f64(double value)
{
	return value < 0.0 ? -value : value;
}

static ef_addr32 current_waypoint(ef_addr32 obj)
{
	return EF_U32(obj + OBJ_WAYPOINTS)
		+ (uint32_t)EF_U8(obj + OBJ_WAYPOINT_INDEX) * WAYPOINT_SIZE;
}

/* Turn OBJ's pitch toward its current waypoint.  While the waypoint is far
 * away, the object aims at the terrain a fixed distance ahead instead when
 * that ground is below it.  The pitch change per frame is limited by the
 * object's pitch rate; small errors are ignored.  Slow hovering objects do
 * not steer at all. */
void steer_pitch_to_waypoint_1dc20(ef_addr32 obj)
{
	ef_addr32 wp;
	double scale, dx, dy, aim_x, aim_y, target_z;
	double sq_x, dist, err, mag, limit;

	if (EF_U8(obj + OBJ_KIND) == KIND_HOVER
	    && !(EF_F64(obj + OBJ_SPEED) >= HOVER_MIN_SPEED))
		return;

	wp = current_waypoint(obj);
	scale = MAP_SCALE;
	dx = (double)EF_S32(wp + WP_X) * scale - EF_F64(obj + OBJ_X);
	dy = (double)EF_S32(wp + WP_Y) * scale - EF_F64(obj + OBJ_Y);

	if (abs_f64(dx) > NEAR_DISTANCE || abs_f64(dy) > NEAR_DISTANCE) {
		double cos_heading = guest_fn_b45a2(EF_F64(obj + OBJ_HEADING));
		double sin_heading = guest_fn_b45ac(EF_F64(obj + OBJ_HEADING));
		double ahead_x = cos_heading * LOOKAHEAD;
		double cos_pitch = guest_fn_b45a2(EF_F64(obj + OBJ_PITCH));
		int32_t cell_x, cell_y;
		int32_t height;

		aim_x = (ahead_x * cos_pitch + EF_F64(obj + OBJ_X)) * CELL_SCALE;
		aim_y = (LOOKAHEAD * sin_heading * cos_pitch + EF_F64(obj + OBJ_Y))
			* CELL_SCALE;
		cell_x = (int32_t)truncate_9d72c(aim_x);
		cell_y = (int32_t)truncate_9d72c(aim_y);
		height = (int32_t)cell_distance_at_7b16c(0, cell_y, cell_x);
		target_z = (double)(-height) * MAP_SCALE + TERRAIN_CLEARANCE;
		if (target_z > EF_F64(obj + OBJ_Z)) {
			aim_x = dx;
			aim_y = dy;
			target_z = EF_F64(current_waypoint(obj) + WP_Z)
				* WAYPOINT_Z_SCALE;
		}
	} else {
		aim_x = dx;
		aim_y = dy;
		target_z = EF_F64(wp + WP_Z) * WAYPOINT_Z_SCALE;
	}

	if (aim_x == 0.0 || aim_y == 0.0)
		return;

	sq_x = guest_fn_b3fd0(aim_x);
	dist = guest_fn_a4470(guest_fn_b3fd0(aim_y) + sq_x);
	err = guest_fn_b4620(EF_F64(obj + OBJ_Z) - target_z, dist)
		- EF_F64(obj + OBJ_PITCH);
	mag = abs_f64(err);
	if (!(mag > MIN_PITCH_ERROR))
		return;

	limit = EF_F64(obj + OBJ_PITCH_RATE) * FRAME_DT * PITCH_RATE_SCALE;
	if (mag > limit)
		mag = limit;
	if (0.0 >= err)
		EF_F64(obj + OBJ_PITCH) -= mag;
	else
		EF_F64(obj + OBJ_PITCH) += mag;
}
