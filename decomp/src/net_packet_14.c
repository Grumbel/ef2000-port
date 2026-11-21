/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network packet 0x14: the full state of a remote unit's grid record. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define NET_SENDER 0x1a6668u
#define UNIT_STRIDE 0xca
#define EXTERNAL_STRIDE 0x45
#define UNIT_LINKS 7

#define POSITION_SCALE 0x185c0cu  /* 2.4384: packet units to world units */
#define ANGLE_PI 0x185c14u        /* pi */
#define ANGLE_WORD_SCALE 0x185c1cu /* 2^-15 */
#define ANGLE_TWO_PI 0x185c24u    /* 2 pi */
#define ANGLE_PER_DEGREE 0x185c2cu /* 1/180 */
#define ANGLE_MINUS_PI 0x185c34u  /* -pi */

/* The record is looked up afresh for every field, as in the original. */
static ef_addr32 unit_record(ef_addr32 data)
{
	return g_world_grid_base + (int32_t)EF_S16(data + 1) * UNIT_STRIDE;
}

void net_packet_14_554b0(ef_addr32 data)
{
	uint8_t i;
	uint8_t side;
	int32_t target;
	ef_addr32 object;

	EF_U8(unit_record(data) + 0x1a) = EF_U8(data + 0x0f);
	EF_F64(unit_record(data) + 0x61) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x0b);
	EF_F64(unit_record(data) + 0x69) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x03);
	EF_F64(unit_record(data) + 0x71) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x07);
	EF_U8(unit_record(data)) = EF_U8(NET_SENDER);

	for (i = 0; i < UNIT_LINKS; i++) {
		EF_U16(unit_record(data) + 0x50 + 2 * i) = EF_U16(data + 0x10 + 2 * i);
		object = lookup_filtered_indexed_object(EF_S16(unit_record(data) + 0x50 + 2 * i));
		if (object != 0)
			EF_U8(object + 0x2a) = i;
	}

	EF_U8(unit_record(data) + 0x5f) = EF_U8(data + 0x1e);
	EF_U8(unit_record(data) + 0x60) = EF_U8(data + 0x1f);
	EF_U16(unit_record(data) + 0x24) = EF_U16(data + 0x20);
	side = EF_U8(data + 0x22);
	EF_U8(unit_record(data) + 0x26) = side;

	/* Claim the target record for this unit when it is in range. */
	target = EF_S16(data + 0x20);
	if (side == 0) {
		if (target != 0xff && g_unit_count > EF_S16(data + 0x20))
			EF_U16(g_world_grid_base + target * UNIT_STRIDE + 0x22) = EF_U16(data + 1);
	} else if (side == 1) {
		if (target != 0xff && g_map_row_count > EF_S16(data + 0x20))
			EF_U8(g_external_grid_base + target * EXTERNAL_STRIDE + 1) = EF_U8(data + 1);
	}

	EF_U16(unit_record(data) + 0xb9) = EF_U16(data + 0x23);
	EF_U8(unit_record(data) + 0xbb) = EF_U8(data + 0x25);
	EF_U8(unit_record(data) + 0x20) = EF_U8(data + 0x26);
	EF_U8(unit_record(data) + 0x8e) = EF_U8(data + 0x27);
	EF_U8(unit_record(data) + 0x5e) = EF_U8(data + 0x28);
	EF_F64(unit_record(data) + 0x27) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x29);
	EF_F64(unit_record(data) + 0x2f) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x2d);
	EF_F64(unit_record(data) + 0x37) = EF_F64(POSITION_SCALE) * (double)EF_S32(data + 0x31);
	EF_F64(unit_record(data) + 0xc1) =
		(double)EF_S16(data + 0x35) * EF_F64(ANGLE_PI) * EF_F64(ANGLE_WORD_SCALE);
	EF_U8(unit_record(data) + 0xbc) = EF_U8(data + 0x37);
	/* Degrees to radians in -pi..pi, truncated and kept as a byte. */
	EF_F64(unit_record(data) + 0x81) = (double)(uint8_t)(int32_t)truncate_9d72c(
		(double)EF_U8(data + 0x38) * EF_F64(ANGLE_TWO_PI) * EF_F64(ANGLE_PER_DEGREE)
		+ EF_F64(ANGLE_MINUS_PI));
}
