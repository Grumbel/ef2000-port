/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CROSS_TRACK 0x1f6f98u
#define TURN_ERROR 0x1f6f60u
#define ALONG_TRACK 0x1f70a0u
#define CONTROL_B0 0x1f70b0u
#define DESIRED_HEADING 0x1f70f8u
#define SPEED_DEMAND 0x1f7100u
#define CONTROL_110 0x1f7110u
#define CAPTURE_DISTANCE 20.0
#define TURN_SLOWDOWN 0x186832u   /* 3.0 */
#define ONE_OVER_PI 0x18683au     /* 1/pi */
#define MAX_SPEED_DEMAND 0x186842u /* 10.0 */
#define SPEED_FALLOFF 0x18684au   /* 0.05 */

/* Steer the current object onto the line through `point` with direction
   `heading`: the desired heading turns towards the line by the
   cross-track error, and the speed demand rises with the distance along
   the line and falls with the turn still to make. */
void steer_onto_line_70914(ef_addr32 point, double heading)
{
	ef_addr32 self = g_current_object;
	double dy = EF_F64(point + 8) - EF_F64(self + 0x51);
	double sin_h = guest_fn_b45ac(heading);
	double cos_h = guest_fn_b45a2(heading);
	double dx = EF_F64(point) - EF_F64(self + 0x49);
	double along = dx * cos_h + dy * sin_h;
	double limit;

	EF_F64(CROSS_TRACK) = dy * cos_h - dx * sin_h;
	EF_F64(DESIRED_HEADING) = guest_fn_b4620(EF_F64(CROSS_TRACK), CAPTURE_DISTANCE) + heading;
	(void)guest_fn_b415c(point, g_current_object + 0x49); /* result unused */
	EF_F64(ALONG_TRACK) = along;

	limit = 1.0 / (guest_fn_b427c_x(EF_F64(TURN_ERROR)) * EF_F64(TURN_SLOWDOWN) * EF_F64(ONE_OVER_PI) + 1.0)
	        * EF_F64(MAX_SPEED_DEMAND);
	EF_F64(CONTROL_B0) = 0.0;
	EF_F64(SPEED_DEMAND) = limit * (1.0 - guest_fn_dcb96_x(-EF_F64(ALONG_TRACK) * EF_F64(SPEED_FALLOFF)));
	guest_fn_6e8d8_x(0u, 0u, 0u, 0u, 2.5);
	EF_F64(CONTROL_110) = 0.0;
	compute_turn_rate_6eaf4(0u, 0u, 0u, 0u, 1.3);
	store_scaled_angle_difference_6ebbc();
}
