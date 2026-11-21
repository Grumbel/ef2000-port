/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CURRENT_UNIT_INDEX ((int16_t)(EF_S32(0x001f71c6u) >> 16))
#define GRID_RECORD_SIZE 0xca

/* Current object fields. */
#define OBJ_FLAGS 0x24
#define OBJ_FLAG_LOW 0x40
#define OBJ_CELL 0x26 /* cell index in the high half */
#define OBJ_X 0x49
#define OBJ_Y 0x51
#define OBJ_Z 0x59
#define OBJ_VZ 0x99
#define OBJ_PHASE 0x16e
#define OBJ_TOUCHDOWN 0x18d
#define OBJ_ENTRY_POINT 0x20b

#define CELL_ENTRY_INDEX 0x5f

#define TOUCHDOWN_Z 0x1284fcu     /* -3.9 */
#define LOW_MARGIN 0x12847cu      /* 5.0 */
#define TARGET_Z 0x1868b9u        /* -60.0 */
#define CLIMB_GAIN 0x1868c1u      /* 0.15 */
#define FINAL_DISTANCE 0x1868c9u  /* 10.0 */
#define CLIMB_DEMAND 0x1f6f68u
#define ALONG_TRACK 0x1f70a0u
#define SPEED_DEMAND 0x1f7100u

#define APPROACH_FINAL 3
#define UNIT_STATE_LANDED 0x0c

/* One step of the approach phases (2 and 3) of the current object: mark
   it low once below the touchdown height plus the margin, report the
   touchdown when it is below the touchdown height with a vertical speed
   under 1, steer onto the line towards its cell's entry point, set the
   climb demand and switch to the final phase (no speed demand) inside
   the final distance. */
void update_approach_710a4(void)
{
	ef_addr32 self = g_current_object;
	uint8_t entry = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(self + OBJ_CELL) >> 16) * GRID_RECORD_SIZE +
	                      CELL_ENTRY_INDEX);
	double point[3];
	double heading;

	if ((EF_U8(self + OBJ_FLAGS) & OBJ_FLAG_LOW) == 0 &&
	    !(EF_F64(TOUCHDOWN_Z) - EF_F64(LOW_MARGIN) > EF_F64(self + OBJ_Z)))
		EF_U8(self + OBJ_FLAGS) |= OBJ_FLAG_LOW;

	self = g_current_object;
	if (EF_F64(self + OBJ_Z) > EF_F64(TOUCHDOWN_Z) && 1.0 > guest_fn_b427c_x(EF_F64(self + OBJ_VZ))) {
		EF_U8(g_current_object + OBJ_TOUCHDOWN) = 1;
		transition_indexed_object_state(CURRENT_UNIT_INDEX, UNIT_STATE_LANDED);
	}

	scaled_entry_point_7ca8((ef_addr32)(uintptr_t)point, entry, EF_U8(g_current_object + OBJ_ENTRY_POINT));
	self = g_current_object;
	heading = guest_fn_b4620(point[1] - EF_F64(self + OBJ_Y), point[0] - EF_F64(self + OBJ_X));
	steer_onto_line_70914((ef_addr32)(uintptr_t)point, heading);

	self = g_current_object;
	EF_F64(CLIMB_DEMAND) = (EF_F64(TARGET_Z) - EF_F64(self + OBJ_Z)) * EF_F64(CLIMB_GAIN);
	if (!(EF_F64(ALONG_TRACK) >= EF_F64(FINAL_DISTANCE)))
		EF_U8(self + OBJ_PHASE) = APPROACH_FINAL;
	self = g_current_object;
	if (EF_U8(self + OBJ_PHASE) == APPROACH_FINAL) {
		EF_U32(SPEED_DEMAND) = 0;
		EF_U32(SPEED_DEMAND + 4) = 0;
		EF_F64(CLIMB_DEMAND) = -EF_F64(self + OBJ_Z) * EF_F64(CLIMB_GAIN);
	}
}
