/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define BOX_SCALE 0x1950c0u /* double 2.0 */

/* Double the first two doubles of a measurement record. */
static void double_pair(ef_addr32 record)
{
	EF_F64(record + 8u) = EF_F64(record + 8u) * EF_F64(BOX_SCALE);
	EF_F64(record) = EF_F64(record) * EF_F64(BOX_SCALE);
}

/* Derive the geometry records from the source box 0x433768: the segment
   and extent records, three measurement records (scaled by 2), two more
   from 0x4334e0 / 0x433408, the three plane blends and the coefficient
   record. */
void derive_geometry_records_109ec0(void)
{
	build_segment_record_10a078(0x4335b8u, 0x433768u);
	build_extent_record_10a030(0x433690u, 0x433768u);
	box_record_fill_10a0d4(0x432ec0u, 0x433768u);
	box_record_fill_10a0d4(0x432d40u, 0x433690u);
	box_record_fill_10a0d4(0x432ce8u, 0x4335b8u);
	double_pair(0x432ec0u);
	double_pair(0x432d40u);
	double_pair(0x432ce8u);
	box_record_fill_10a0d4(0x432e70u, 0x4334e0u);
	double_pair(0x432e70u);
	box_record_fill_10a0d4(0x432e18u, 0x433408u);
	double_pair(0x432e18u);
	interp_blend_10a164(0x432de0u, 0x432f60u, 0x4335b8u);
	interp_blend_10a164(0x432e10u, 0x432fb8u, 0x4334e0u);
	interp_blend_10a164(0x432e68u, 0x432ff8u, 0x433408u);
	derive_coefficients_10a1c4(0x432de8u, 0x432fa0u);
}
