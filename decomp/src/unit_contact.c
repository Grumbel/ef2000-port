/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define UNIT_TABLE        0x0019bb58u /* pointer to 0xca-byte unit records */
#define UNIT_RECORD_SIZE  0xcau
#define ACTIVE_UNIT       0x0019bb5eu /* word: active unit index, 0xff = none */
#define PLAYER_UNIT       0x00197264u /* word */
#define REPLY_ODDS        0x00197269u /* byte: 1-in-N chance of an event reply */
#define CONTACT_SOUND_ON  0x00121e5cu /* double: nonzero enables the contact sound */

#define NO_UNIT           0xff

/* Fields shared by both records. */
#define REC_SIDE          0x15u /* word */
#define REC_KIND          0x1au /* byte */
#define REC_CONTACT       0x21u /* byte */
#define REC_TASK          0x22u /* word: 0xff when idle */
#define REC_UNIT          0x50u /* word: unit index */
#define REC_SOUND         0x5fu /* byte: 0xff = none */
#define REC_POS           0x61u /* two doubles: x, y */

#define KIND_NONE         0
#define KIND_ESCORT       3
#define KIND_AUDIBLE      7
#define KIND_IGNORED      12

void guest_fn_7030(ef_addr32 a, ef_addr32 b); /* 0x7030 EAX/EDX */

/*
 * 0x73ec, 430 bytes. Unit A (EAX) has come into contact with object B (EDX).
 * Nothing happens when both are on the same side or B's kind is none or
 * ignored. Otherwise B is marked as contacted, may play its contact sound,
 * may get a task assigned, may answer with a message or event when it lies
 * in a map cell of A's side, and finally escorts and radio calls are handled.
 */
void handle_unit_contact_73ec(ef_addr32 a, ef_addr32 b)
{
	int16_t cell_point[2];
	int16_t active;
	uint8_t kind;

	if (EF_U16(a + REC_SIDE) == EF_U16(b + REC_SIDE))
		return;
	kind = EF_U8(b + REC_KIND);
	if (kind == KIND_NONE || kind == KIND_IGNORED)
		return;

	cell_point[0] = (int16_t)double_to_scaled_b4250(EF_F64(b + REC_POS));
	cell_point[1] = (int16_t)double_to_scaled_b4250(EF_F64(b + REC_POS + 8u));
	(void)guest_fn_b415c(a + REC_POS, b + REC_POS); /* distance, unused */

	EF_U8(b + REC_CONTACT) = 1;
	if (EF_U8(b + REC_KIND) == KIND_AUDIBLE && EF_U8(b + REC_SOUND) != 0xff &&
	    EF_F64(CONTACT_SOUND_ON) != 0.0)
		guest_fn_47764_x((int32_t)EF_S16(a + REC_UNIT), 0x39u, (int32_t)EF_U8(b + REC_SOUND), 0u, 0u);

	active = EF_S16(ACTIVE_UNIT);
	if (active != NO_UNIT) {
		ef_addr32 rec = EF_U32(UNIT_TABLE) + (uint32_t)(int32_t)active * UNIT_RECORD_SIZE;

		if (EF_S16(rec + REC_TASK) == NO_UNIT) {
			if (EF_U16(PLAYER_UNIT) == EF_U16(ACTIVE_UNIT))
				assign_task_71fc(a, b);
		} else if (EF_S16(b + REC_TASK) == NO_UNIT) {
			assign_task_71fc(a, b);
		}
	}

	if ((uint16_t)guest_fn_85bc8_x((ef_addr32)(uintptr_t)cell_point) == EF_U16(a + REC_SIDE)) {
		if (EF_S16(a + REC_SIDE) == 2 &&
		    (int32_t)(uint16_t)guest_random_word() % (int32_t)EF_U8(REPLY_ODDS) == 0)
			dispatch_unit_event_46254((uint32_t)(int32_t)EF_S16(a + REC_UNIT),
			                          (int32_t)EF_S16(b + REC_UNIT), 0x40u, 0u, 0u, 0u);
		else if (EF_S16(b + REC_TASK) == NO_UNIT)
			maybe_fire_message_7308(a, b);
	}

	if (EF_U8(b + REC_KIND) == KIND_ESCORT)
		guest_fn_7030(a, b);
	if (EF_S16(b + REC_TASK) != NO_UNIT)
		maybe_radio_call_6f48(a, b);
}
