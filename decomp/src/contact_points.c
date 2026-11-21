/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CONTACT_RECORDS 0x4ddee0u /* pointer to five 0xd5-byte records */
#define CONTACT_RECORD_SIZE 0xd5
#define CONTACT_COUNT 5
#define ACCUMULATE_TOTALS 0x433985u
#define TOTAL_FORCE 0x432950u /* three doubles */
#define TOTAL_MOMENT 0x432910u /* three doubles */
#define SHARED_RECORD_VALUE 0x432990u
#define INPUT_A 0x432da0u
#define INPUT_B 0x432200u
#define INPUT_C 0x432ad8u
#define INPUT_D 0x432c28u

/* Per-record work vectors, laid out as the original stack frame. */
struct contact_frame {
	double rotation[9];
	double projected[3];
	double moment[3];
	double force[3];
};

static ef_addr32 contact_record(int index)
{
	return EF_U32(CONTACT_RECORDS) + (uint32_t)index * CONTACT_RECORD_SIZE;
}

#define FRAME_ADDR(field) ((ef_addr32)(uintptr_t)(field))

/* Update the five contact records: optionally clear the total force
   and moment, share record 2's value with records 2 and 3 when record 2
   is active, then compute each record's force and moment, rotate them
   into the record's frame and accumulate them (0x11c350). */
void update_contact_points_11c20c(void)
{
	struct contact_frame frame;
	int i;

	if (EF_U8(ACCUMULATE_TOTALS) != 0) {
		for (i = 0; i < 3; i++) {
			EF_F64(TOTAL_FORCE + 8 * i) = 0.0;
			EF_F64(TOTAL_MOMENT + 8 * i) = 0.0;
		}
	}
	if (EF_U8(contact_record(2) + 0x68) != 0) {
		double value = EF_F64(SHARED_RECORD_VALUE);

		EF_F64(EF_U32(contact_record(2) + 0x69) + 0x20) = value;
		EF_F64(EF_U32(contact_record(3) + 0x69) + 0x20) = value;
	}

	for (i = 0; i < CONTACT_COUNT; i++) {
		project_triple_11c484(FRAME_ADDR(frame.projected), contact_record(i));
		guest_fn_11c584(FRAME_ADDR(frame.force), FRAME_ADDR(frame.moment), contact_record(i),
		                EF_F64(INPUT_A), EF_F64(INPUT_B), EF_F64(INPUT_C), EF_F64(INPUT_D));
		build_matrix_11c4ec(FRAME_ADDR(frame.rotation), contact_record(i));
		accumulate_contact_load_11c350(FRAME_ADDR(frame.force), FRAME_ADDR(frame.moment), FRAME_ADDR(frame.rotation),
		                contact_record(i));
	}
}
