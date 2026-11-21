/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <string.h>

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CAM_DISTANCE_RATE EF_F64(0x001f6f70u)
#define CAM_BEARING_Y     EF_F64(0x001f6f78u)
#define CAM_HEIGHT_OFFSET EF_F64(0x001f6f80u)
#define CAM_BANK          EF_F64(0x001f6fd0u)
#define CAM_BASE_DISTANCE EF_F64(0x001f6fe0u)
#define CAM_HEADING_BIAS  EF_F64(0x001f6ff0u)
#define CAM_SHAKE         EF_F64(0x001f7050u)
#define CAM_ROLL          0x001f7060u          /* double passed by halves */
#define CAM_TARGET_HEIGHT EF_F64(0x001f7088u)
#define CAM_FAR_PLANE     EF_F64(0x001f70a0u)
#define CAM_HEADING       EF_F64(0x001f70f8u)
#define CAM_DISTANCE      EF_F64(0x001f7100u)
#define CAM_BEARING_X     EF_F64(0x001f71b0u)
#define CAM_BANK_ANGLE    EF_F64(0x001f70e0u)
#define CAM_HEADING_DELTA EF_F64(0x001f70f0u)
#define CAM_HEADING_REF   EF_F64(0x001f7130u)
#define CAM_HEIGHT_DELTA  EF_F64(0x001f70a8u)
#define CAM_PITCH         EF_F64(0x001f70b0u)
#define CAM_BANK_LIMIT    EF_F64(0x001f7098u)
#define CAM_BANK_TARGET   EF_F64(0x001f7110u)
#define CAM_TURN_LIMIT    EF_F64(0x001f7138u)
#define CAM_LIFT          EF_F64(0x001f71a0u)
#define CAM_MASS_SCALE    EF_F64(0x001f7160u)
#define CAM_AREA_SCALE    EF_F64(0x001f7158u)
#define GROUND_HEIGHT     EF_F64(0x001284d4u)
#define AIR_DENSITY       EF_F64(0x0012849cu)

#define AIRCRAFT_TYPES     0x00128599u         /* 0xe5-byte rows */
#define AIRCRAFT_TYPE_SIZE 0xe5u

#define OBJ_Y              0x59
#define OBJ_TYPE           0xe9                /* uint8 aircraft type */
#define OBJ_MODE           0x16c               /* uint8 */
#define OBJ_SUBMODE        0x16e               /* uint8 */
#define OBJ_ROW            0x28                /* int16 world grid row */
#define OBJ_MIN_PITCH      0x69                /* double */

#define GRID_ROW_SIZE      0xca
#define GRID_ROW_SECONDARY 0x50                /* uint16 */

#define CAM_PITCH_MAX      0.7853981634000001  /* about pi/4, 0x3fe921fb544486e1 */

enum { MODE_HOVER = 0x10 };

#define GRAVITY 9.81

/* 0x6e3e0, 443 bytes (ret 0x10). Follow the player aircraft with the
   chase camera. GROW_OUT and GROW_IN scale the per-frame distance change
   when the camera rate is negative or positive. The distance restarts at
   the base distance, a hovering aircraft keeps at least one unit beyond
   it, and the aircraft type caps it. The bank term is
   tan(bank) * g / base distance, or 0 for a base distance <= 1. */
void update_chase_camera_6e3e0(double grow_out, double grow_in)
{
	uint32_t unit = g_current_object;
	double rate, limit, bank;
	uint32_t bank_bits[2];

	CAM_TARGET_HEIGHT = EF_F64(unit + OBJ_Y) + CAM_HEIGHT_OFFSET;
	CAM_DISTANCE = CAM_BASE_DISTANCE;
	rate = CAM_DISTANCE_RATE;
	if (!(0.0 >= rate)) {
		CAM_DISTANCE = rate * grow_in + CAM_DISTANCE;
		if (EF_U8(unit + OBJ_MODE) == MODE_HOVER) {
			limit = 1.0 + CAM_BASE_DISTANCE;
			if (!(CAM_DISTANCE >= limit))
				CAM_DISTANCE = limit;
		}
	} else {
		CAM_DISTANCE = rate * grow_out + CAM_DISTANCE;
	}
	CAM_HEADING = atan2_b4620(CAM_BEARING_Y, CAM_BEARING_X * 15.0) + CAM_HEADING_BIAS;

	limit = EF_F64(AIRCRAFT_TYPES + EF_U8(g_current_object + OBJ_TYPE) * AIRCRAFT_TYPE_SIZE);
	if (!(CAM_DISTANCE < limit))
		CAM_DISTANCE = limit;
	CAM_FAR_PLANE = 2000.0;
	CAM_SHAKE = 1.0;

	if (1.0 < CAM_BASE_DISTANCE)
		bank = guest_fn_b45fa(CAM_BANK) * GRAVITY / CAM_BASE_DISTANCE;
	else
		bank = 0.0;
	memcpy(bank_bits, &bank, sizeof bank_bits);
	/* 0x6ebe0 takes five doubles as ten stack dwords: 3.0, 1.3, bank,
	   roll, far plane. */
	guest_fn_6ebe0_x(0u, 0x40080000u, 0xcccccccdu, 0x3ff4ccccu,
	    bank_bits[0], bank_bits[1], EF_U32(CAM_ROLL), EF_U32(CAM_ROLL + 4u),
	    EF_U32(0x001f70a0u), EF_U32(0x001f70a4u));
}

/* 0x6ebe0, 916 bytes (ret 0x28). Limit the chase camera pitch and bank.
   Nothing happens below a bearing of 1. The pitch is the angle to the
   target height minus PITCH_BIAS; above 3000 units below ground it is
   held above a terrain limit (doubling DAMP_RATE when that limit exceeds
   the object's minimum pitch), then it is clamped to +-pi/4. When the
   lift ratio lies inside (-1, 1) the bank target follows the heading
   delta within +-the turn limit and +-0.95 acos(ratio); otherwise the bank
   target is zeroed. */
void update_camera_pitch_6ebe0(double damp_rate, double turn_rate,
	double bank_bias, double pitch_bias, double range)
{
	ef_addr32 obj;
	int16_t unit;
	double limit;
	double ratio;
	double bank;

	if (1.0 > CAM_BEARING_X)
		return;
	CAM_HEADING_DELTA = wrap_angle_b42f0(CAM_HEADING - CAM_HEADING_REF);
	obj = g_current_object;
	CAM_HEIGHT_DELTA = CAM_TARGET_HEIGHT - EF_F64(obj + OBJ_Y);
	CAM_PITCH = pitch_bias - atan2_b4620(CAM_HEIGHT_DELTA, range);
	if (EF_F64(obj + OBJ_Y) > -3000.0) {
		unit = (int16_t)g_grid_secondary_selector;  /* low word at 0x1f71c8 */
		if (EF_U8(obj + OBJ_MODE) == 9 && EF_U8(obj + OBJ_SUBMODE) == 3) {
			if (!(CAM_TARGET_HEIGHT >= -130.0))
				limit = guest_fn_82478(unit, -100.0);
			else
				limit = -0.175;
		} else if (!(GROUND_HEIGHT + -20.0 >= EF_F64(obj + OBJ_Y))) {
			CAM_PITCH = CAM_PITCH_MAX;
			limit = 0.0;
			CAM_HEADING_DELTA = 0.0;
		} else {
			int16_t row = EF_U16(obj + OBJ_ROW);
			uint16_t secondary = EF_U16(g_world_grid_base
				+ (int32_t)row * GRID_ROW_SIZE + GRID_ROW_SECONDARY);
			limit = guest_fn_82478(unit,
				(int32_t)unit == (int32_t)secondary ? -200.0 : -50.0);
		}
		if (!(CAM_PITCH >= limit)) {
			CAM_PITCH = limit;
			if (limit > EF_F64(obj + OBJ_MIN_PITCH))
				damp_rate *= 2.0;
		}
	}
	if (CAM_PITCH > CAM_PITCH_MAX)
		CAM_PITCH = CAM_PITCH_MAX;
	if (!(CAM_PITCH >= -CAM_PITCH_MAX))
		CAM_PITCH = -CAM_PITCH_MAX;

	ratio = (CAM_LIFT * 2.0 * GRAVITY)
		/ (((CAM_BEARING_X * (AIR_DENSITY * CAM_BEARING_X)) * CAM_MASS_SCALE)
			* CAM_AREA_SCALE);
	if (1.0 > abs_double_b427c(ratio)) {
		CAM_BANK_LIMIT = acos_neg_e7df4(ratio) * 0.95;
		bank = sin_b45ac(CAM_HEADING_DELTA * 0.5) * CAM_TURN_LIMIT * CAM_SHAKE
			+ bank_bias;
		if (bank > CAM_TURN_LIMIT)
			bank = CAM_TURN_LIMIT;
		if (!(bank >= -CAM_TURN_LIMIT))
			bank = -CAM_TURN_LIMIT;
		CAM_BANK_TARGET = atan2_b4620(bank * CAM_BEARING_X, GRAVITY);
		if (CAM_BANK_TARGET > CAM_BANK_LIMIT)
			CAM_BANK_TARGET = CAM_BANK_LIMIT;
		if (!(CAM_BANK_TARGET >= -CAM_BANK_LIMIT))
			CAM_BANK_TARGET = -CAM_BANK_LIMIT;
		guest_fn_6e8d8_x(0u, 0u, 0u, 0u, damp_rate);
	} else {
		CAM_BANK_TARGET = 0.0;
		CAM_BANK_ANGLE = -CAM_TURN_LIMIT * (1.0 / 3.0);
	}
	compute_turn_rate_6eaf4(0u, 0u, 0u, 0u, turn_rate);
}
