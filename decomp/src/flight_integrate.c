/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U16(a) (*(uint16_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define ADDR(p)   ((ef_addr32)(uintptr_t)(p))

#define FRAME_TICKS       0x00127d92u  /* uint16 */
#define TICK_SECONDS      EF_F64(0x00181978u)
#define FRAME_DT          EF_F64(0x0019af70u)
#define HOVER_SPEED_LIMIT EF_F64(0x00181980u)
#define HOVER_DRIFT       EF_F64(0x00181988u)
#define HOVER_ACCEL_SCALE EF_F64(0x00181990u)

#define OBJ_KIND          0x41
#define OBJ_POSITION      0x51   /* double[3] */
#define OBJ_SPEED         0x71   /* double */
#define OBJ_MAX_SPEED     0x79   /* double */
#define OBJ_HEADING       0x81   /* double */
#define OBJ_ACCEL         0x91   /* double */
#define OBJ_PITCH         0x99   /* double */

enum { KIND_WAYPOINT_PROJECTILE = 8, KIND_HOVER = 16 };

/* Advance OBJ by one frame: slew its aim, accelerate toward the maximum
 * speed, move it along its heading and pitch, store the new speed and
 * refresh the integer mirror.  Hovering objects below the speed limit drift
 * along the heading, accelerate more slowly and climb at half rate. */
void integrate_flight_1d954(ef_addr32 obj)
{
	double vel[3];
	double drift_x = 0.0, drift_z = 0.0;
	double accel = EF_F64(obj + OBJ_ACCEL);
	double climb_rate = 1.0;
	double dt, heading;
	double cos_heading, sin_heading, cos_pitch;

	dt = (double)(int32_t)EF_U16(FRAME_TICKS) * TICK_SECONDS;
	FRAME_DT = dt;

	if ((EF_U32(obj + OBJ_SPEED + 4) & 0x7fffffffu) != 0 || EF_U32(obj + OBJ_SPEED) != 0)
		slew_aim_angles_1db08(obj);

	if (EF_U8(obj + OBJ_KIND) == KIND_HOVER && EF_F64(obj + OBJ_SPEED) < HOVER_SPEED_LIMIT) {
		heading = EF_F64(obj + OBJ_HEADING);
		drift_x = guest_fn_b45a2(heading) * HOVER_DRIFT;
		drift_z = guest_fn_b45ac(heading) * HOVER_DRIFT;
		accel = EF_F64(obj + OBJ_ACCEL) * HOVER_ACCEL_SCALE;
		climb_rate = 0.5;
	}

	steer_pitch_to_waypoint_1dc20(obj);

	if (EF_F64(obj + OBJ_SPEED) < EF_F64(obj + OBJ_MAX_SPEED))
		EF_F64(obj + OBJ_SPEED) = accel * FRAME_DT + EF_F64(obj + OBJ_SPEED);

	/* The helpers are called first so each product stays on the FPU stack;
	 * a spilled partial product would flush to a denormal like the original
	 * never does. */
	cos_heading = guest_fn_b45a2(EF_F64(obj + OBJ_HEADING));
	cos_pitch = guest_fn_b45a2(EF_F64(obj + OBJ_PITCH));
	vel[0] = cos_heading * EF_F64(obj + OBJ_SPEED) * cos_pitch;
	sin_heading = guest_fn_b45ac(EF_F64(obj + OBJ_HEADING));
	cos_pitch = guest_fn_b45a2(EF_F64(obj + OBJ_PITCH));
	vel[1] = sin_heading * EF_F64(obj + OBJ_SPEED) * cos_pitch;
	vel[2] = -guest_fn_b45ac(EF_F64(obj + OBJ_PITCH)) * EF_F64(obj + OBJ_SPEED);

	dt = FRAME_DT;
	EF_F64(obj + OBJ_POSITION) = (vel[0] + drift_x) * dt + EF_F64(obj + OBJ_POSITION);
	EF_F64(obj + OBJ_POSITION + 8) = (vel[1] + drift_z) * dt + EF_F64(obj + OBJ_POSITION + 8);
	EF_F64(obj + OBJ_POSITION + 16) = dt * vel[2] * climb_rate + EF_F64(obj + OBJ_POSITION + 16);

	EF_F64(obj + OBJ_SPEED) = vector_length_b4364(ADDR(vel));
	mirror_record_floats_1debc(obj);

	if (EF_U8(obj + OBJ_KIND) == KIND_WAYPOINT_PROJECTILE && waypoint_near_1d8e8(obj))
		projectile_ground_test_1df90(obj);
}
