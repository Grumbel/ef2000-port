/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define AXIS_SWAP_MATRIX 0x433088u /* 3x3 doubles */

/* Reset the flight model: load the axis swap matrix (x <-> z), the time
   constants and the geometry (0x10c1b0), then clear the accumulators and
   rates and copy the derived reference values. */
void reset_flight_model_e78b4(void)
{
	static const double axis_swap[9] = {
		0.0, 0.0, 1.0,
		0.0, 1.0, 0.0,
		1.0, 0.0, 0.0,
	};
	uint32_t i;

	for (i = 0; i < 9u; ++i)
		EF_F64(AXIS_SWAP_MATRIX + i * 8u) = axis_swap[i];
	init_time_constants_e7850();
	for (i = 0; i < 3u; ++i)
		EF_F64(0x432000u + i * 8u) = 0.0;
	guest_fn_10c1b0();

	EF_F64(0x432f30u) = EF_F64(0x432f28u);
	EF_F64(0x432698u) = 0.8;
	EF_F64(0x432720u) = 0.025;
	for (i = 0; i < 3u; ++i) {
		EF_F64(0x432338u + i * 8u) = 0.0;
		EF_F64(0x432028u + i * 8u) = 0.0;
	}
	EF_F64(0x432158u) = 0.0;
	EF_F64(0x15fa7cu) = 0.0;
	EF_F64(0x433038u) = 0.0;
	EF_U8(0x43399cu) = 0;
	EF_F64(0x432ce0u) = EF_F64(0x432cd8u);
	EF_F64(0x432f38u) = EF_F64(0x433060u);
	EF_F64(0x432f58u) = EF_F64(0x433060u);
}
