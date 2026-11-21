/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Body-frame force and moment contributions (three doubles each). */
#define CONTACT_FORCE 0x432950u
#define FORCE_B 0x4329a0u
#define FORCE_C 0x432a28u
#define FORCE_D 0x432a50u
#define CONTACT_MOMENT 0x432910u
#define MOMENT_B 0x432978u
#define MOMENT_C 0x4329c0u
#define MOMENT_D 0x4329f8u

#define MASS 0x433050u
#define ATTITUDE 0x464480u      /* 3x3 doubles, rows 0x18 bytes apart */
#define DOWN (ATTITUDE + 0x30u) /* third row: gravity direction in the body frame */
#define GRAVITY 0x195070u       /* 9.81 */
#define INV_GRAVITY 0x195078u   /* 1 / 9.81 */
#define SPEED 0x432338u
#define TURN_TERM 0x432268u
#define FREEZE_ROLL_MOMENT 0x43397cu

#define BODY_FORCE 0x432a90u
#define WORLD_FORCE 0x432a78u
#define NORMAL_LOAD 0x432f40u
#define BODY_MOMENT 0x432b08u

static double sum4(ef_addr32 a, ef_addr32 b, ef_addr32 c, ef_addr32 d, uint32_t axis)
{
	axis *= 8;
	return EF_F64(a + axis) + EF_F64(b + axis) + EF_F64(c + axis) + EF_F64(d + axis);
}

/* Run the table initialisers and the two coefficient stages, then total
   the body forces with gravity, rotate the total into the world frame,
   derive the normal load factor and total the moments (the contact roll
   moment cleared first while 0x43397c is set). */
void total_forces_1098f0(void)
{
	double mass, fz;

	run_table_initialisers_1196a0();
	guest_fn_119be0();
	guest_fn_11a6e4();

	mass = EF_F64(MASS);
	EF_F64(BODY_FORCE) = sum4(CONTACT_FORCE, FORCE_B, FORCE_C, FORCE_D, 0) + EF_F64(DOWN) * mass * EF_F64(GRAVITY);
	EF_F64(BODY_FORCE + 8) =
		sum4(CONTACT_FORCE, FORCE_B, FORCE_C, FORCE_D, 1) + EF_F64(DOWN + 8) * mass * EF_F64(GRAVITY);
	fz = sum4(CONTACT_FORCE, FORCE_B, FORCE_C, FORCE_D, 2);
	EF_F64(BODY_FORCE + 16) = fz;
	EF_F64(NORMAL_LOAD) =
		-fz / (mass * EF_F64(GRAVITY)) + EF_F64(SPEED) * EF_F64(SPEED) * EF_F64(TURN_TERM) * EF_F64(INV_GRAVITY);
	EF_F64(BODY_FORCE + 16) = mass * EF_F64(DOWN + 16) * EF_F64(GRAVITY) + EF_F64(BODY_FORCE + 16);
	matrix_mul_vector_b4550(WORLD_FORCE, BODY_FORCE, ATTITUDE);

	if (EF_U8(FREEZE_ROLL_MOMENT) != 0) {
		EF_U32(CONTACT_MOMENT) = 0;
		EF_U32(CONTACT_MOMENT + 4) = 0;
	}
	EF_F64(BODY_MOMENT) = sum4(CONTACT_MOMENT, MOMENT_B, MOMENT_C, MOMENT_D, 0);
	EF_F64(BODY_MOMENT + 8) = sum4(CONTACT_MOMENT, MOMENT_B, MOMENT_C, MOMENT_D, 1);
	EF_F64(BODY_MOMENT + 16) = sum4(CONTACT_MOMENT, MOMENT_B, MOMENT_C, MOMENT_D, 2);
}
