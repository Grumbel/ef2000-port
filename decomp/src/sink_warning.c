/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define HEIGHT_NOW 0x432198u     /* double */
#define HEIGHT_BEFORE 0x432218u  /* double */
#define DRIVE_A 0x432800u        /* double, tested for non-zero */
#define DRIVE_B 0x432810u        /* double, tested for non-zero */
#define FACTOR 0x4327c0u         /* double */
#define HOLD_COUNTER 0x4327f8u   /* double */
#define FACTOR_LOAD 0x433050u    /* double */
#define FACTOR_BASE 0x432a90u    /* double */
#define WARNING_ENABLED 0x433993u
#define WARNING_ACTIVE 0x433991u

#define WARNING_CONTROL 0x2d

/* The original tests the bits of the double, so -0.0 counts as zero and
   a NaN as non-zero. */
static int is_nonzero(ef_addr32 value)
{
	return (EF_U32(value + 4u) & 0x7fffffffu) != 0 || EF_U32(value) != 0;
}

/* Update the factor from the height change while driven, and switch the
   warning control 0x2d on while the height drops by more than 5 per step
   (when enabled) and off otherwise. */
void update_sink_warning_10958c(void)
{
	if (is_nonzero(DRIVE_A) || is_nonzero(DRIVE_B)) {
		if (!is_nonzero(HOLD_COUNTER)) {
			EF_F64(FACTOR) = (EF_F64(HEIGHT_NOW) - EF_F64(HEIGHT_BEFORE)) * 0.01 + EF_F64(FACTOR);
			EF_F64(FACTOR) = EF_F64(FACTOR) - EF_F64(FACTOR_BASE) / (EF_F64(FACTOR_LOAD) * 25.0);
		}
		if (EF_F64(FACTOR) < 0.6)
			EF_F64(FACTOR) = 0.6;
		if (is_nonzero(HOLD_COUNTER)) {
			EF_F64(FACTOR) = 1.0;
			if (EF_F64(HEIGHT_NOW) - EF_F64(HEIGHT_BEFORE) < -5.0)
				EF_F64(HOLD_COUNTER) = EF_F64(HOLD_COUNTER) + -1.0;
		}
	}

	if (EF_F64(HEIGHT_NOW) - EF_F64(HEIGHT_BEFORE) < -5.0 && EF_U8(WARNING_ENABLED) != 0) {
		if (EF_U8(WARNING_ACTIVE) != 1)
			claim_ui_slot_801e4(WARNING_CONTROL, 1, 0);
		EF_U8(WARNING_ACTIVE) = 1;
	} else {
		if (EF_U8(WARNING_ACTIVE) != 0)
			claim_ui_slot_801e4(WARNING_CONTROL, 1, 0);
		EF_U8(WARNING_ACTIVE) = 0;
	}
}
