/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CELL_STRIDE 0xca
#define NODE_STRIDE 0x45
#define NO_TRACK 0xff
#define REPORT_COUNTDOWN 0x126358u
#define REPORT_INTERVAL 0x226
#define TRACK_EVENT 0x3c
#define RANGE_SCALE 0x183561u /* 0.000539: world units to nautical miles */

static ef_addr32 grid_cell(uint32_t cell)
{
	return g_world_grid_base + cell * CELL_STRIDE;
}

static ef_addr32 tracked_cell(int16_t node)
{
	return grid_cell(EF_U8(g_external_grid_base + (int32_t)node * NODE_STRIDE + 1));
}

/* Keep external node `node`'s track on a cell of `unit`'s side: drop the
   track when the cell is no longer a kind 3 cell bound to this node in
   state 1; otherwise every 550 ticks report its bearing and range. */
void update_external_track_3a000(int16_t unit, int16_t node)
{
	ef_addr32 cell = tracked_cell(node);
	int32_t countdown;
	int16_t bearing;
	int16_t range;

	if (EF_U16(grid_cell((uint32_t)(int32_t)unit) + 0x15) != EF_U16(cell + 0x15))
		return;
	if (EF_U8(cell + 0x1a) != 3 || EF_U16(cell + 0x24) != (uint16_t)node || EF_U8(cell + 0x26) != 1) {
		EF_U8(g_external_grid_base + (int32_t)node * NODE_STRIDE + 1) = NO_TRACK;
		return;
	}

	countdown = EF_S32(REPORT_COUNTDOWN) - (int32_t)g_simulation_tick_step;
	EF_S32(REPORT_COUNTDOWN) = countdown;
	if (countdown >= 0)
		return;
	EF_S32(REPORT_COUNTDOWN) = REPORT_INTERVAL;

	bearing = (int16_t)scale_to_angle_45fc4(report_point_offset_37c0c(cell + 0x61, node));
	range = (int16_t)(int32_t)truncate_9d72c(scaled_pair_distance_37bc8(tracked_cell(node) + 0x61, node)
	                                         * EF_F64(RANGE_SCALE));
	dispatch_unit_event_46254((uint32_t)(int32_t)EF_S16(grid_cell((uint32_t)(int32_t)unit) + 0x50),
	                          EF_S16(tracked_cell(node) + 0x50), TRACK_EVENT,
	                          (uint32_t)(int32_t)bearing, (uint32_t)(int32_t)range, 0);
}
