/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Surface record fields (doubles). */
#define SURFACE_SPAN 0x08
#define SURFACE_SCALE 0x18
#define SURFACE_TABLE_AXIS 0x20
#define SURFACE_ANGLE 0x38

#define CUBIC_TABLE 0x17e3ccu /* three-axis table descriptors */
#define SQUARE_TABLE 0x17e4ecu
#define SCALE_LIMIT 0x195964u /* 4.0 */
#define ANGLE_LIMIT 0x19596cu /* 40 degrees in radians */
#define OFFSET_BIAS 0x195974u /* -0.15 */
#define OFFSET_GAIN 0x19597cu /* 4.0 */
#define OFFSET_SLOPE 0x195984u /* 0.21 */
#define OFFSET_BASE 0x19598cu /* 0.19 */
#define HALF 0x195994u /* 0.5 */

#define FRAME_ADDR(field) ((ef_addr32)(uintptr_t)(field))

/* Lookup point and per-axis lower-cell flags for 0x11acb0. */
struct table_query {
	double axis[3];
	uint8_t cell[3];
};

/* Look up the two coefficient tables at the surface's table axis, its
   scale times the factor of `mach` (0x11af14) and 0.0, and return
   cubic * eta^3 + square * eta^2. The second and third cell flags
   select the table region from the scaled value and from the angle
   atan2(tan(surface angle), factor). */
double surface_coefficient_11bcc4(ef_addr32 surface, double eta, double mach)
{
	struct table_query query;
	double factor = sqrt_branch_11af14(mach);
	double angle;
	double cubic;
	double square;
	double cubic_term;

	query.axis[0] = EF_F64(surface + SURFACE_TABLE_AXIS);
	query.axis[1] = factor * EF_F64(surface + SURFACE_SCALE);
	angle = guest_fn_dc1e9_st(guest_fn_b45fa(EF_F64(surface + SURFACE_ANGLE)), factor);
	query.cell[0] = 0;
	query.cell[1] = 0;
	if (query.axis[1] > EF_F64(SCALE_LIMIT))
		query.cell[1] = 1;
	query.cell[2] = 0;
	if (!(0.0 >= angle)) {
		query.cell[2] = 1;
		if (angle > EF_F64(ANGLE_LIMIT))
			query.cell[2] = 2;
	}
	query.axis[2] = 0.0;
	cubic = interpolate_table_11acb0(CUBIC_TABLE, FRAME_ADDR(&query.axis), FRAME_ADDR(&query.cell));
	square = interpolate_table_11acb0(SQUARE_TABLE, FRAME_ADDR(&query.axis), FRAME_ADDR(&query.cell));
	cubic_term = pow_b3fec(eta, 3.0) * cubic;
	return pow_b3fec(eta, 2.0) * square + cubic_term;
}

/* Difference of the surface coefficient between the outer (points[2])
   and inner (points[0]) span stations, each taken relative to the half
   span, divided by the factor of `mach` and scaled by
   ((offset[0] - 0.15) * 4 * 0.21 + 0.19). `unused` is never read. */
double surface_coefficient_delta_11bdc0(ef_addr32 surface, ef_addr32 offset, ef_addr32 points, double mach,
                                        double unused)
{
	double factor = sqrt_branch_11af14(mach);
	double scale = (EF_F64(offset) + EF_F64(OFFSET_BIAS)) * EF_F64(OFFSET_GAIN) * EF_F64(OFFSET_SLOPE) +
	               EF_F64(OFFSET_BASE);
	double outer = surface_coefficient_11bcc4(
		surface, EF_F64(points + 0x10) / (EF_F64(surface + SURFACE_SPAN) * EF_F64(HALF)), mach);
	double inner = surface_coefficient_11bcc4(
		surface, EF_F64(points) / (EF_F64(surface + SURFACE_SPAN) * EF_F64(HALF)), mach);

	(void)unused;
	return (outer - inner) / factor * scale;
}
