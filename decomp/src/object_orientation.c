/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rebuild an object's orientation matrix and view position from its
   angle words and integer position. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define ANGLE_PI 0x186b74u          /* pi */
#define ANGLE_WORD_SCALE 0x186b7cu  /* 1/32767 */
#define POSITION_SCALE 0x186b84u    /* 2.4384 */
#define POSITION_FRACTION 0x186b8cu /* 1/256 */
#define EYE_FORWARD 0x186b94u       /* 3.48343 */
#define EYE_UP 0x186b9cu            /* -1.06825 */

/* Object fields: +0/+4/+8 int32 x/y/z, +0xc/+0xe/+0x10 int16 angles,
   +0x61/+0x69/+0x71 angles in radians, +0xa1..+0xe1 the 3x3 matrix,
   +0xe9 no-eye-offset flag, +0x49/+0x51/+0x59 view position. */
void update_object_orientation_73ee0(int16_t index)
{
	ef_addr32 object = lookup_filtered_indexed_object(index);
	double pi, unit;
	double cos_a, sin_a, cos_b, sin_b, cos_c, sin_c;
	double cos_a_sin_c, sin_a_sin_c;
	double scale, fraction, x, y, z;

	if (object == 0)
		return;

	pi = EF_F64(ANGLE_PI);
	unit = EF_F64(ANGLE_WORD_SCALE);
	EF_F64(object + 0x69) = (double)-(int32_t)EF_S16(object + 0x0c) * pi * unit;
	EF_F64(object + 0x61) = (double)EF_S16(object + 0x0e) * pi * unit;
	EF_F64(object + 0x71) = (double)-(int32_t)EF_S16(object + 0x10) * pi * unit;

	cos_a = guest_fn_b45a2(EF_F64(object + 0x61));
	sin_a = guest_fn_b45ac(EF_F64(object + 0x61));
	cos_b = guest_fn_b45a2(EF_F64(object + 0x71));
	sin_b = guest_fn_b45ac(EF_F64(object + 0x71));
	cos_c = guest_fn_b45a2(EF_F64(object + 0x69));
	sin_c = guest_fn_b45ac(EF_F64(object + 0x69));

	sin_a_sin_c = sin_a * sin_c;
	cos_a_sin_c = cos_a * sin_c;
	EF_F64(object + 0xa1) = cos_a * cos_c;
	EF_F64(object + 0xa9) = cos_a_sin_c * sin_b - sin_a * cos_b;
	EF_F64(object + 0xb1) = cos_a_sin_c * cos_b + sin_a * sin_b;
	EF_F64(object + 0xb9) = sin_a * cos_c;
	EF_F64(object + 0xc1) = sin_a_sin_c * sin_b + cos_a * cos_b;
	EF_F64(object + 0xc9) = sin_a_sin_c * cos_b - cos_a * sin_b;
	EF_F64(object + 0xd1) = -sin_c;
	EF_F64(object + 0xd9) = sin_b * cos_c;
	EF_F64(object + 0xe1) = cos_b * cos_c;

	scale = EF_F64(POSITION_SCALE);
	fraction = EF_F64(POSITION_FRACTION);
	x = (double)EF_S32(object + 0x00) * scale * fraction;
	y = (double)EF_S32(object + 0x04) * scale * fraction;
	z = (double)EF_S32(object + 0x08) * scale * fraction;
	if (EF_U8(object + 0xe9) == 0) {
		/* Offset the eye along the object's axes. */
		double forward = EF_F64(EYE_FORWARD);
		double up = EF_F64(EYE_UP);
		z -= EF_F64(object + 0xa1) * forward + EF_F64(object + 0xb1) * up;
		x -= EF_F64(object + 0xb9) * forward + EF_F64(object + 0xc9) * up;
		y -= EF_F64(object + 0xd1) * forward + EF_F64(object + 0xe1) * up;
	}
	EF_F64(object + 0x49) = z;
	EF_F64(object + 0x51) = x;
	EF_F64(object + 0x59) = y;
}
