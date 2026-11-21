/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <string.h>

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define PLAYER_UNIT_WORD 0x1f71cau
#define PREVIOUS_POSITION 0x1a5bf4u
#define POSITION 0x1a5be8u
#define GROUND_HEIGHT 0x1a2f28u
#define REPLAY_ACTIVE 0x1a364eu
#define TIME_SKIP_ALLOWED 0x127d9cu
#define TIME_SKIP_ACTIVE 0x125935u
#define TIME_SKIP_TICKS 0x12afd4u
#define FAST_TIME 0x1a3634u
#define FRAME_SECONDS 0x1a5b30u
#define AIRSPEED 0x4331c3u
#define AIRSPEED_KNOTS 0x1a5be4u
#define KNOTS_PER_METRE_SECOND 0x185b94u /* 1.942 */
#define HEIGHT_PROBE_DEPTH (-60.96) /* metres (200 ft) */
#define MAX_SKIP_TICKS 10

static void advance_simulation_time(double seconds)
{
	uint32_t halves[2];

	memcpy(halves, &seconds, sizeof halves);
	store_args_and_shutdown_e474c(0, 0, 0, 0, halves[0], halves[1]);
}

/* One frame of the player's flight model: remember the last position,
   probe the ground height, advance simulation time (a replay, a time
   skip in ticks of 1/50 s or 1/200 s, or the frame time split into one
   or four steps) and update the airspeed in knots. */
void advance_player_frame_53ecc(void)
{
	EF_U32(PREVIOUS_POSITION + 4) = EF_U32(POSITION + 4);
	EF_U32(PREVIOUS_POSITION) = EF_U32(POSITION + 8);
	EF_U32(PREVIOUS_POSITION + 8) = EF_U32(POSITION);
	guest_fn_819b0_x();
	EF_F64(GROUND_HEIGHT) = guest_fn_82478(EF_S16(PLAYER_UNIT_WORD), HEIGHT_PROBE_DEPTH);

	if (EF_U8(REPLAY_ACTIVE) != 0) {
		guest_fn_53c80_x();
	} else if (EF_U8(TIME_SKIP_ALLOWED) != 0 && EF_U8(TIME_SKIP_ACTIVE) != 0) {
		int32_t ticks_per_second = 50;
		int32_t ticks = EF_U16(TIME_SKIP_TICKS);

		if (EF_U8(FAST_TIME) != 0) {
			ticks_per_second = 200;
			ticks *= 4;
		}
		while (ticks > 0) {
			int32_t chunk = ticks > MAX_SKIP_TICKS ? MAX_SKIP_TICKS : ticks;

			advance_simulation_time((double)chunk / (double)ticks_per_second);
			ticks -= chunk;
		}
	} else {
		int32_t steps = EF_U8(FAST_TIME) != 0 ? 4 : 1;
		double seconds = EF_F64(FRAME_SECONDS) / (double)steps;
		int32_t i;

		for (i = 0; i < steps; i++)
			advance_simulation_time(seconds);
	}

	EF_S32(AIRSPEED_KNOTS) = (int32_t)truncate_9d72c(EF_F64(AIRSPEED) * EF_F64(KNOTS_PER_METRE_SECOND));
}
