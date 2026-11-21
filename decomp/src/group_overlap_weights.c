/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* List entries. */
#define ENTRY_NEXT 0x04
#define ENTRY_GROUP 0x0b /* byte */
#define ENTRY_KIND 0x0c  /* byte */
#define ENTRY_X 0x11
#define ENTRY_Y 0x15
#define ENTRY_WEIGHT 0x27 /* double */

#define KIND_UNIT 0x31
#define TABLE_TAG 0x18f380u

/* The original tests the double's bits, so -0.0 counts as zero. */
static int is_nonzero(ef_addr32 value)
{
	return (EF_U32(value + 4u) & 0x7fffffffu) != 0 || EF_U32(value) != 0;
}

/* Weigh every unit entry of the list by the summed distances to the
   friendly unit entries (0xcc1ac) that are within their range (0xcbfa0),
   total the weights per group in a temporary table and finally store
   each unit's weight divided by its group total. The weight is cleared
   before that division, so every unit ends up with a zero (signed like
   the group total) weight; this is the original behaviour. */
void update_group_weights_cbff0(ef_addr32 list_holder, ef_addr32 groups)
{
	ef_addr32 entry = EF_U32(list_holder);
	uint32_t size = (uint32_t)((int32_t)(int16_t)guest_fn_bc5d0_x(groups) << 3);
	ef_addr32 totals = guest_fn_cc6c0_x(size, TABLE_TAG);

	fill_bytes_b3fb0(totals, 0, size);

	for (; entry != 0; entry = EF_U32(entry + ENTRY_NEXT)) {
		ef_addr32 other;
		ef_addr32 total;

		EF_U32(entry + ENTRY_WEIGHT) = 0;
		EF_U32(entry + ENTRY_WEIGHT + 4u) = 0;
		for (other = EF_U32(list_holder); other != 0; other = EF_U32(other + ENTRY_NEXT)) {
			double distance;

			if ((uint16_t)group_flag_negative_cc1ac(EF_U8(entry + ENTRY_GROUP), EF_U8(other + ENTRY_GROUP), groups) == 0
			    || EF_U8(entry + ENTRY_KIND) != KIND_UNIT || EF_U8(other + ENTRY_KIND) != KIND_UNIT)
				continue;
			distance = int_distance_2d_f83d0(EF_S32(entry + ENTRY_X), EF_S32(other + ENTRY_X),
			                                 EF_S32(entry + ENTRY_Y), EF_S32(other + ENTRY_Y));
			if ((double)(int16_t)default_range_cbfa0(other) >= distance)
				EF_F64(entry + ENTRY_WEIGHT) = EF_F64(entry + ENTRY_WEIGHT) + distance;
		}
		total = totals + (uint32_t)EF_U8(entry + ENTRY_GROUP) * 8u;
		EF_F64(total) = EF_F64(entry + ENTRY_WEIGHT) + EF_F64(total);
	}

	for (entry = EF_U32(list_holder); entry != 0; entry = EF_U32(entry + ENTRY_NEXT)) {
		ef_addr32 total = totals + (uint32_t)EF_U8(entry + ENTRY_GROUP) * 8u;

		EF_U32(entry + ENTRY_WEIGHT) = 0;
		EF_U32(entry + ENTRY_WEIGHT + 4u) = 0;
		if (is_nonzero(total) && EF_U8(entry + ENTRY_KIND) == KIND_UNIT) {
			EF_F64(entry + ENTRY_WEIGHT) = EF_F64(entry + ENTRY_WEIGHT) / EF_F64(total);
		} else if (EF_U8(entry + ENTRY_KIND) == KIND_UNIT) {
			EF_U32(entry + ENTRY_WEIGHT) = 0;
			EF_U32(entry + ENTRY_WEIGHT + 4u) = 0;
		}
	}

	if (totals != 0)
		guest_fn_cc768(totals);
}
