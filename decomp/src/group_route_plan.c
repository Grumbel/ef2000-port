/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/*
 * Builds the waypoint list of a group sent from a home position to a
 * target: out to the target, through a jittered midpoint, back home and
 * out to the target again.  The list at group+0x20 holds 25-byte
 * waypoints.
 */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define WAYPOINT_SIZE    0x19

/* waypoint fields */
#define WP_X             0x00   /* int32 */
#define WP_Y             0x04   /* int32 */
#define WP_ALTITUDE      0x08   /* word */
#define WP_HEADING       0x0a   /* word */
#define WP_SPEED         0x0c   /* word */
#define WP_ENTRY         0x0e   /* word */
#define WP_WORD_10       0x10
#define WP_WORD_12       0x12
#define WP_WORD_14       0x14
#define WP_TIME          0x16   /* word */
#define WP_ACTION        0x18   /* byte */

/* group fields */
#define GROUP_TYPE       0x02   /* byte */
#define GROUP_STEP       0x1a   /* word */
#define GROUP_COUNT      0x1c   /* word */
#define GROUP_WORD_1E    0x1e
#define GROUP_ROUTE      0x20   /* waypoint list */

/* target fields */
#define TARGET_KIND      0x0b   /* byte */
#define TARGET_X         0x11
#define TARGET_Y         0x15
#define TARGET_ALTITUDE  0x1d
#define TARGET_HEADING   0x1f

#define ROUTE(group)     EF_U32((group) + GROUP_ROUTE)
#define WP(group, n)     (ROUTE(group) + (n) * WAYPOINT_SIZE)

#define MIDPOINT_MIN     0x19
#define MIDPOINT_MAX     0x176
#define DEFAULT_TIME     0x1c2

static int32_t random_below_30(void)
{
	return (int32_t)(uint16_t)step_generator_a45b3() % 30;
}

/* Places waypoint n halfway between home and target, shifted by a random
 * amount (x 20..49 down, y 0..29 up).  Only for target kinds 0..5; the
 * original switches on the kind with two identical case groups. */
static void jitter_midpoint(ef_addr32 group, int n, ef_addr32 target,
		int32_t home_x, int32_t home_y)
{
	int32_t mid;

	if (EF_U8(target + TARGET_KIND) > 5)
		return;
	mid = (home_x + EF_S32(target + TARGET_X)) / 2;
	EF_S32(WP(group, n) + WP_X) = mid - (random_below_30() + 20);
	mid = (home_y + EF_S32(target + TARGET_Y)) / 2;
	EF_S32(WP(group, n) + WP_Y) = random_below_30() + mid;
}

static int32_t clamp_midpoint(int32_t value)
{
	if (value > MIDPOINT_MAX)
		return MIDPOINT_MAX;
	if (value < MIDPOINT_MIN)
		return MIDPOINT_MIN;
	return value;
}

static int is_strike_group(ef_addr32 group)
{
	uint8_t type = EF_U8(group + GROUP_TYPE);

	return type == 2 || type == 0xa;
}

static uint16_t cruise_speed(ef_addr32 group)
{
	return EF_U8(group + GROUP_TYPE) == 3 ? 0xfa : 5;
}

/* Clears the leg words of waypoint n and sets its time and action. */
static void finish_waypoint(ef_addr32 group, int n, uint8_t action)
{
	EF_U16(WP(group, n) + WP_WORD_10) = 0;
	EF_U16(WP(group, n) + WP_WORD_12) = 0;
	EF_U16(WP(group, n) + WP_WORD_14) = 0;
	EF_U16(WP(group, n) + WP_ENTRY) = 0;
	EF_U16(WP(group, n) + WP_TIME) = DEFAULT_TIME;
	EF_U8(WP(group, n) + WP_ACTION) = action;
}

static void copy_target_position(ef_addr32 group, int n, ef_addr32 target)
{
	EF_U32(WP(group, n) + WP_X) = EF_U32(target + TARGET_X);
	EF_U32(WP(group, n) + WP_Y) = EF_U32(target + TARGET_Y);
	EF_U16(WP(group, n) + WP_ALTITUDE) = EF_U16(target + TARGET_ALTITUDE);
	EF_U16(WP(group, n) + WP_HEADING) = EF_U16(target + TARGET_HEADING);
}

static void set_home(ef_addr32 group, int n, int32_t home_x, int32_t home_y,
		uint16_t altitude, uint16_t heading)
{
	EF_S32(WP(group, n) + WP_X) = home_x;
	EF_S32(WP(group, n) + WP_Y) = home_y;
	EF_U16(WP(group, n) + WP_ALTITUDE) = altitude;
	EF_U16(WP(group, n) + WP_HEADING) = heading;
}

void plan_group_route_b914c(ef_addr32 target, int32_t home_x, int32_t home_y,
		uint16_t home_altitude, uint16_t home_heading, ef_addr32 group,
		int16_t first_entry, uint8_t escorted, ef_addr32 slot_head)
{
	int midpoint;

	/* 0: the target */
	copy_target_position(group, 0, target);
	EF_U16(WP(group, 0) + WP_SPEED) = 0;
	EF_U16(WP(group, 0) + WP_WORD_10) = 0;
	EF_U16(WP(group, 0) + WP_WORD_12) = 0;
	EF_U16(WP(group, 0) + WP_WORD_14) = 0;
	EF_U8(WP(group, 0) + WP_ACTION) = 1;
	EF_U16(WP(group, 0) + WP_ENTRY) = (uint16_t)find_free_entry(first_entry, target);

	if (escorted != 0) {
		/* 1: an escort slot, 2: the midpoint kept on the map */
		fill_group_slot_b513c(group, 1, target, slot_head);
		midpoint = 2;
		jitter_midpoint(group, midpoint, target, home_x, home_y);
		EF_S32(WP(group, 2) + WP_X) = clamp_midpoint(EF_S32(WP(group, 2) + WP_X));
		EF_S32(WP(group, 2) + WP_Y) = clamp_midpoint(EF_S32(WP(group, 2) + WP_Y));
	} else {
		midpoint = 1;
		jitter_midpoint(group, midpoint, target, home_x, home_y);
	}

	/* midpoint, home, target */
	EF_U16(WP(group, midpoint) + WP_ALTITUDE) = 0;
	EF_U16(WP(group, midpoint) + WP_HEADING) = 0;
	finish_waypoint(group, midpoint, is_strike_group(group) ? 3 : 0);
	set_home(group, midpoint + 1, home_x, home_y, home_altitude, home_heading);
	EF_U16(WP(group, midpoint) + WP_SPEED) = cruise_speed(group);
	EF_U16(WP(group, midpoint + 1) + WP_SPEED) = cruise_speed(group);
	finish_waypoint(group, midpoint + 1, is_strike_group(group) ? 8 : 0);
	copy_target_position(group, midpoint + 2, target);
	EF_U16(WP(group, midpoint + 2) + WP_SPEED) = 0;
	finish_waypoint(group, midpoint + 2, 2);

	EF_U16(group + GROUP_WORD_1E) = 0;
	EF_U16(group + GROUP_STEP) = 0;
	EF_U16(group + GROUP_COUNT) = escorted != 0 ? (uint16_t)(escorted + 4) : 4;
}
