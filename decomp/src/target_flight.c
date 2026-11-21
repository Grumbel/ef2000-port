/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define GRID_RECORD_SIZE 0xca
#define CELL_FLIGHT 0xb5        /* target flight record pointer */
#define CELL_FLIGHT_MODE 0xbc
#define CELL_FLIGHT_SPEED 0xc1  /* double */
#define FLIGHT_WAYPOINTS 0x18   /* int16 x, y, z offsets, 6 bytes each */
#define WAYPOINT_SIZE 6
#define OBJ_CELL 0x26           /* cell index in the high half */
#define OBJ_WAYPOINT 0x2a
#define WAYPOINT_SCALE 0x18678au /* 2.4384 */
#define NULL_FLIGHT_MESSAGE 0x186774u /* "Null Target Flight %d" */

static ef_addr32 current_cell_record(void)
{
	return EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(g_current_object + OBJ_CELL) >> 16) * GRID_RECORD_SIZE;
}

/* Fly the current object towards the active waypoint of its cell's
   target flight: the waypoint is the flight origin plus the scaled
   waypoint offset, on the terrain height there. */
void fly_to_target_waypoint_6f504(void)
{
	double point[3];
	ef_addr32 flight, waypoint, cell;
	double scale;

	if (EF_U32(current_cell_record() + CELL_FLIGHT) == 0)
		guest_fatal_arg(NULL_FLIGHT_MESSAGE, (uint32_t)(EF_S32(g_current_object + OBJ_CELL) >> 16));

	flight = EF_U32(current_cell_record() + CELL_FLIGHT);
	point[0] = word_to_scaled_b4230(EF_S16(flight));
	point[1] = word_to_scaled_b4230(EF_S16(flight + 2));
	point[2] = 0.0;

	waypoint = flight + FLIGHT_WAYPOINTS + (uint32_t)EF_U8(g_current_object + OBJ_WAYPOINT) * WAYPOINT_SIZE;
	scale = EF_F64(WAYPOINT_SCALE);
	point[0] = EF_S16(waypoint) * scale + point[0];
	point[1] = EF_S16(waypoint + 2) * scale + point[1];
	point[2] = lookup_scaled_cell_7b040((ef_addr32)(uintptr_t)point);

	cell = current_cell_record();
	guest_fn_6f758((ef_addr32)(uintptr_t)point, EF_U8(cell + CELL_FLIGHT_MODE), EF_F64(cell + CELL_FLIGHT_SPEED));
}
