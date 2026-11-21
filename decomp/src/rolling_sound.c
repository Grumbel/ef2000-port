/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define GROUND_SPEED 0x1a3078u      /* double, m/s */
#define KNOTS_PER_MPS 0x188718u     /* double 1 / 0.515 */
#define ROLL_KNOTS 0x12b084u        /* dword, rounded ground speed */
#define ROLL_SOUND_ON 0x12b088u     /* byte, rolling loop (control 3) active */
#define ROLL_START_PLAYED 0x12b089u /* byte, start cue (control 2) played */
#define ROLL_PITCH_KNOTS 0x12b0ccu  /* dword, speed last sent to the voices */
#define SOUND_ENABLED 0x126528u     /* byte */
#define SOUND_BANK 0x1f9078u        /* dword, bank whose +0xc record is the loop sample */
#define VOICE_KIND 0x1f9088u        /* 10 bytes, 3 = rolling loop */

#define UNIT_ON_GROUND 0x18du       /* byte */
#define UNIT_FLAGS 0x24u            /* byte, 0x10 = sound suppressed */

#define ROLL_START_CONTROL 2
#define ROLL_LOOP_CONTROL 3
#define ROLL_VOICES 10

/* 0x7f850, 401 bytes. Ground rolling sound for UNIT (EAX). On the ground
   the speed is floored to whole knots; above 5 knots the start cue is
   claimed once (sound enabled, unit not suppressed), above 10 knots the
   rolling loop is claimed with the loop sample's pitch word temporarily set
   to the speed, and while it runs the rolling voices follow speed changes.
   Off the ground, suppressed or at 10 knots and below, the loop is
   released. */
void update_rolling_sound_7f850(ef_addr32 unit)
{
	ef_addr32 sample;
	int32_t saved_pitch;
	int i;

	if (unit == 0)
		return;
	if (EF_U8(unit + UNIT_ON_GROUND) == 0) {
		if (EF_U8(ROLL_SOUND_ON) != 0) {
			ui_release_matching_control(ROLL_LOOP_CONTROL);
			EF_U8(ROLL_SOUND_ON) = 0;
			EF_U8(ROLL_START_PLAYED) = 0;
		}
		return;
	}
	EF_S32(ROLL_KNOTS) = (int32_t)truncate_9d72c(
		guest_fn_e86f7_x(EF_F64(GROUND_SPEED) * EF_F64(KNOTS_PER_MPS)));

	if (EF_U8(ROLL_START_PLAYED) == 0 && EF_U8(SOUND_ENABLED) != 0
	    && (EF_U8(unit + UNIT_FLAGS) & 0x10u) == 0 && EF_S32(ROLL_KNOTS) > 5) {
		claim_ui_slot_801e4(ROLL_START_CONTROL, 1, 1);
		EF_U8(ROLL_START_PLAYED) = 1;
	}

	if (EF_U8(ROLL_SOUND_ON) == 0) {
		if ((EF_U8(unit + UNIT_FLAGS) & 0x10u) != 0 || EF_S32(ROLL_KNOTS) <= 10)
			return;
		sample = EF_U32(EF_U32(SOUND_BANK) + 0xcu);
		saved_pitch = get_high_word_d771c(sample);
		set_word_d7714(sample, (uint16_t)EF_U32(ROLL_KNOTS));
		claim_ui_slot_801e4(ROLL_LOOP_CONTROL, 0, 1);
		set_word_d7714(EF_U32(EF_U32(SOUND_BANK) + 0xcu), (uint16_t)saved_pitch);
		EF_U8(ROLL_SOUND_ON) = 1;
		return;
	}

	if ((EF_U8(unit + UNIT_FLAGS) & 0x10u) == 0 && EF_S32(ROLL_KNOTS) > 10) {
		if (EF_S32(ROLL_KNOTS) == EF_S32(ROLL_PITCH_KNOTS))
			return;
		for (i = 0; i < ROLL_VOICES; ++i) {
			if (EF_U8(VOICE_KIND + (uint32_t)i) == 3) {
				channel_call_102858_d73b8((uint8_t)i, EF_U32(ROLL_KNOTS));
				EF_U32(ROLL_PITCH_KNOTS) = EF_U32(ROLL_KNOTS);
			}
		}
		return;
	}
	ui_release_matching_control(ROLL_LOOP_CONTROL);
	EF_U8(ROLL_SOUND_ON) = 0;
}
