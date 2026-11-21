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

#define AIRCRAFT_TYPES     0x00128599u         /* 0xe5-byte rows */
#define AIRCRAFT_TYPE_SIZE 0xe5u

#define OBJ_Y              0x59
#define OBJ_TYPE           0xe9                /* uint8 aircraft type */
#define OBJ_MODE           0x16c               /* uint8 */

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
