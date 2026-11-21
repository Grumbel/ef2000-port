/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Three-axis table descriptor: three dimension bytes, then pointers to
   the three ascending axis arrays and to the values (doubles). */
#define TABLE_AXIS(n) (3 + 4 * (n))

/* Fraction of `value` between the axis entries cell and cell + 1. */
static double axis_fraction(ef_addr32 table, int axis, uint8_t cell, double value)
{
	ef_addr32 entry = EF_U32(table + TABLE_AXIS(axis)) + (uint32_t)cell * 8;

	return (value - EF_F64(entry)) / (EF_F64(entry + 8) - EF_F64(entry));
}

static double blend(double t, double low, double high)
{
	return t * high + (1.0 - t) * low;
}

/* Trilinear interpolation of `table` at the three doubles of `point`,
   inside the cell whose lower corner indices are the three bytes of
   `cells`. */
double interpolate_table_11acb0(ef_addr32 table, ef_addr32 point, ef_addr32 cells)
{
	double corner[2][2][2];
	double t;
	uint8_t i;
	uint8_t j;
	uint8_t k;

	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			for (k = 0; k < 2; k++)
				corner[i][j][k] = table_double_11ac3c(table, (uint8_t)(EF_U8(cells) + i),
				                                      (uint8_t)(EF_U8(cells + 1) + j),
				                                      (uint8_t)(EF_U8(cells + 2) + k));
	for (j = 0; j < 2; j++)
		for (k = 0; k < 2; k++) {
			t = axis_fraction(table, 0, EF_U8(cells), EF_F64(point));
			corner[0][j][k] = blend(t, corner[0][j][k], corner[1][j][k]);
		}
	for (k = 0; k < 2; k++) {
		t = axis_fraction(table, 1, EF_U8(cells + 1), EF_F64(point + 8));
		corner[0][0][k] = blend(t, corner[0][0][k], corner[0][1][k]);
	}
	t = axis_fraction(table, 2, EF_U8(cells + 2), EF_F64(point + 0x10));
	return blend(t, corner[0][0][0], corner[0][0][1]);
}
