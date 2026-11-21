/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#include <string.h>

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define ACCUMULATE_TOTALS 0x433985u
#define TOTAL_FORCE 0x432950u /* three doubles */
#define TOTAL_MOMENT 0x432910u /* three doubles */
#define RECORD_FORCE 0xa5 /* three doubles in the record frame */
#define RECORD_MOMENT 0xbd /* three doubles in the record frame */

#define FRAME_ADDR(field) ((ef_addr32)(uintptr_t)(field))

/* Rotate one contact's force and moment into its record frame, add the
   moment of the force about the record's lever arm, store both in the
   record and, when enabled, add them to the totals. */
void accumulate_contact_load_11c350(ef_addr32 force, ef_addr32 moment, ef_addr32 rotation, ef_addr32 record)
{
	double arm[3];
	double arm_moment[3];
	double local_moment[3];
	double local_force[3];
	int i;

	matrix_transpose_mul_vector_b4508(FRAME_ADDR(local_force), force, rotation);
	matrix_transpose_mul_vector_b4508(FRAME_ADDR(local_moment), moment, rotation);
	project_triple_11c484(FRAME_ADDR(arm), record);
	cross_product_doubles(FRAME_ADDR(arm_moment), FRAME_ADDR(arm), FRAME_ADDR(local_force));
	add_vectors_doubles(FRAME_ADDR(local_moment), FRAME_ADDR(local_moment), FRAME_ADDR(arm_moment));
	/* Copied as raw bytes, as the original does with integer moves. */
	memcpy(ef_guest_memory_ptr(record + RECORD_FORCE), local_force, sizeof local_force);
	memcpy(ef_guest_memory_ptr(record + RECORD_MOMENT), local_moment, sizeof local_moment);
	if (EF_U8(ACCUMULATE_TOTALS) != 0) {
		for (i = 0; i < 3; i++) {
			EF_F64(TOTAL_FORCE + 8 * i) += local_force[i];
			EF_F64(TOTAL_MOMENT + 8 * i) += local_moment[i];
		}
	}
}
