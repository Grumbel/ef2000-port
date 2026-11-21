/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define DUMMY_PLANE_UNIT 0x1f71d0u /* signed word */
#define NO_DUMMY_PLANE_MESSAGE 0x186ae4u /* "No Dummy Plane!" */

/* Source record: dword coordinates +0/+4/+8, signed angle words
   +0xc/+0xe/+0x10, dword speed +0x26. */
#define POSITION_SCALE 2.4384           /* 0x186af4 */
#define FIXED_8 0.00390625              /* 0x186afc, 1/256 */
#define ANGLE_PI 3.1415926536           /* 0x186b04 */
#define ANGLE_UNIT 3.051850947599719e-05 /* 0x186b0c, 1/32767 */
#define SPEED_SCALE 0.515               /* 0x186b14 */

/* Object fields (doubles). */
#define OBJ_POS_A 0x49
#define OBJ_POS_B 0x51
#define OBJ_POS_C 0x59
#define OBJ_ANGLE_A 0x61
#define OBJ_ANGLE_C 0x69
#define OBJ_ANGLE_B 0x71
#define OBJ_SPEED 0x91
#define OBJ_MATRIX 0xa1 /* 3x3 */
#define OBJ_RATE 0x150

static double scaled_angle(int32_t value)
{
	return (double)value * ANGLE_PI * ANGLE_UNIT;
}

/* Place the dummy plane object (unit word 0x1f71d0) from the source
   record: scaled position, angles and speed, and the orientation matrix
   built from the three angles. */
void place_dummy_plane_73084(ef_addr32 source)
{
	ef_addr32 object = lookup_indexed_object((int16_t)EF_U16(DUMMY_PLANE_UNIT));
	double ca, sa, cb, sb, cc, sc;
	ef_addr32 m;

	if (object == 0)
		ui_report_missing_record(NO_DUMMY_PLANE_MESSAGE);

	EF_F64(object + OBJ_POS_A) = (double)EF_S32(source + 8u) * POSITION_SCALE * FIXED_8;
	EF_F64(object + OBJ_POS_B) = (double)EF_S32(source) * POSITION_SCALE * FIXED_8;
	EF_F64(object + OBJ_POS_C) = (double)EF_S32(source + 4u) * POSITION_SCALE * FIXED_8;
	EF_F64(object + OBJ_ANGLE_A) = scaled_angle(EF_S16(source + 0xeu));
	EF_F64(object + OBJ_ANGLE_C) = scaled_angle(-(int32_t)EF_S16(source + 0xcu));
	EF_F64(object + OBJ_ANGLE_B) = scaled_angle(-(int32_t)EF_S16(source + 0x10u));
	EF_F64(object + OBJ_SPEED) = (double)EF_S32(source + 0x26u) * SPEED_SCALE * FIXED_8;

	ca = guest_fn_b45a2(EF_F64(object + OBJ_ANGLE_A));
	sa = guest_fn_b45ac(EF_F64(object + OBJ_ANGLE_A));
	cb = guest_fn_b45a2(EF_F64(object + OBJ_ANGLE_B));
	sb = guest_fn_b45ac(EF_F64(object + OBJ_ANGLE_B));
	cc = guest_fn_b45a2(EF_F64(object + OBJ_ANGLE_C));
	sc = guest_fn_b45ac(EF_F64(object + OBJ_ANGLE_C));

	m = object + OBJ_MATRIX;
	EF_F64(m + 0x00u) = ca * cc;
	EF_F64(m + 0x08u) = ca * sc * sb - sa * cb;
	EF_F64(m + 0x10u) = ca * sc * cb + sa * sb;
	EF_F64(m + 0x18u) = sa * cc;
	EF_F64(m + 0x20u) = sa * sc * sb + ca * cb;
	EF_F64(m + 0x28u) = sa * sc * cb - ca * sb;
	EF_F64(m + 0x30u) = -sc;
	EF_F64(m + 0x38u) = cc * sb;
	EF_F64(m + 0x40u) = cc * cb;
	EF_U32(object + OBJ_RATE) = 0;
	EF_U32(object + OBJ_RATE + 4u) = 0;
}
