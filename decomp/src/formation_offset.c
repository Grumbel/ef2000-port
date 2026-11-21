/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <string.h>

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CURRENT_UNIT_INDEX (EF_S32(0x001f71c6u) >> 16)
#define GRID_RECORD_SIZE 0xca
#define CELL_LEAD_UNIT 0x50

/* Current object fields. */
#define OBJ_CELL 0x26
#define OBJ_POS 0x49
#define OBJ_LEADER 0x144 /* leader unit in the high half */
#define OBJ_FORMATION_MODE 0x16c

/* Leader fields. */
#define LEADER_POS 0x49
#define LEADER_HEADING 0x61
#define LEADER_PITCH 0x69
#define LEADER_SPEED 0x71
#define LEADER_ROLL 0x91

#define FOLLOW_SPEED 0x1f6fd0u
#define FOLLOW_ROLL 0x1f6fe0u
#define FOLLOW_HEADING 0x1f6ff0u
#define FOLLOW_PITCH 0x1f7060u
#define FOLLOW_POINT 0x1f7020u
#define ALONG_TRACK 0x1f70a0u
#define STATION_ERROR 0x1f6f70u

static void copy_double(ef_addr32 to, ef_addr32 from)
{
	memcpy(ef_guest_memory_ptr(to), ef_guest_memory_ptr(from), sizeof(double));
}

/* Formation keeping for the current object: take over its leader's
   speed and attitude, measure the distance to the follow point, and set
   the station error to `station` (leader frame) minus the object's
   offset from the leader rotated into the leader's heading. Returns 0
   without a leader, or when the object is in formation mode 1 and its
   cell is led by the current unit. */
uint8_t formation_offset_6e1c4(ef_addr32 station)
{
	ef_addr32 self = g_current_object;
	ef_addr32 leader = guest_fn_7489c(EF_S32(self + OBJ_LEADER) >> 16);
	double offset[3], c, s, along, across;

	if (leader == 0)
		return 0;
	if (EF_U8(self + OBJ_FORMATION_MODE) == 1 &&
	    CURRENT_UNIT_INDEX == (int32_t)EF_U16(EF_U32(0x0019bb58u) +
	                                          (uint32_t)(EF_S32(self + OBJ_CELL) >> 16) * GRID_RECORD_SIZE +
	                                          CELL_LEAD_UNIT))
		return 0;

	EF_F64(FOLLOW_SPEED) = EF_F64(leader + LEADER_SPEED);
	copy_double(FOLLOW_PITCH, leader + LEADER_PITCH);
	copy_double(FOLLOW_HEADING, leader + LEADER_HEADING);
	copy_double(FOLLOW_ROLL, leader + LEADER_ROLL);
	EF_F64(ALONG_TRACK) = distance_2d_b415c(g_current_object + OBJ_POS, FOLLOW_POINT);

	self = g_current_object;
	offset[0] = EF_F64(self + OBJ_POS) - EF_F64(leader + LEADER_POS);
	offset[1] = EF_F64(self + OBJ_POS + 8) - EF_F64(leader + LEADER_POS + 8);
	offset[2] = EF_F64(self + OBJ_POS + 16) - EF_F64(leader + LEADER_POS + 16);
	c = guest_fn_b45a2(EF_F64(leader + LEADER_HEADING));
	s = guest_fn_b45ac(EF_F64(leader + LEADER_HEADING));
	along = s * offset[1] + c * offset[0];
	across = c * offset[1] - s * offset[0];

	EF_F64(STATION_ERROR) = EF_F64(station) - along;
	EF_F64(STATION_ERROR + 8) = EF_F64(station + 8) - across;
	EF_F64(STATION_ERROR + 16) = EF_F64(station + 16) - offset[2];
	return 1;
}
