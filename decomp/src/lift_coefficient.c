/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Lift table doubles (0x432c..). */
#define CL_TERM 0x432cc0u
#define CL_MAX 0x432c40u
#define CL_MIN 0x432ca8u
#define CL_RAW 0x432c70u     /* limited lift coefficient */
#define CL_EXTRA 0x432c78u
#define CL_TOTAL 0x432c18u
#define STALL_RATIO 0x432850u
#define SOFT_STALL 0x433980u /* byte: allow overshoot up to one width */
#define LIMITED 0x433996u    /* byte: the coefficient was limited */
#define LIFT_FORCE 0x432ad0u

/* Build the lift coefficient from the table terms, limit it to the
   maximum (softly, by up to one stall width, when 0x433980 is set) and
   the negative maximum, and derive the lift force
   0.5 * rho * V^2 * area * CL. */
void update_lift_coefficient_1196c8(void)
{
	uint8_t limited = 0;

	EF_F64(CL_TERM) = EF_F64(0x432828u) * EF_F64(0x432c10u) * EF_F64(0x432818u) + EF_F64(0x432ca0u);
	EF_F64(CL_MAX) = EF_F64(0x432828u) * EF_F64(0x432c88u) * EF_F64(0x432818u) + EF_F64(0x432c20u);
	EF_F64(CL_MIN) = -EF_F64(CL_MAX);
	EF_F64(CL_RAW) = EF_F64(0x432c68u) * EF_F64(0x432c28u) + EF_F64(CL_TERM);

	if (EF_F64(CL_RAW) > EF_F64(CL_MAX)) {
		if (EF_U8(SOFT_STALL) != 0) {
			double width = EF_F64(0x432c30u) * EF_F64(0x4328d0u);
			EF_F64(STALL_RATIO) = (EF_F64(CL_RAW) - EF_F64(CL_MAX)) / width;
			if (1.0 < EF_F64(STALL_RATIO)) {
				EF_F64(STALL_RATIO) = 1.0;
				limited = 1;
				EF_F64(CL_RAW) = EF_F64(CL_MAX) + width;
			}
		} else {
			EF_F64(CL_RAW) = EF_F64(CL_MAX);
			EF_F64(STALL_RATIO) = 0.0;
			limited = 1;
		}
	}
	if (EF_F64(CL_RAW) < EF_F64(CL_MIN))
		EF_F64(CL_RAW) = EF_F64(CL_MIN);

	EF_F64(CL_EXTRA) = EF_F64(0x432cd0u) * EF_F64(0x432990u) * EF_F64(0x432968u);
	EF_F64(CL_TOTAL) = EF_F64(CL_RAW) + EF_F64(CL_EXTRA);
	EF_F64(LIFT_FORCE) = EF_F64(0x432da0u) * 0.5 * EF_F64(0x432278u) * EF_F64(0x433058u) * EF_F64(CL_TOTAL);
	EF_U8(LIMITED) = limited;
}
