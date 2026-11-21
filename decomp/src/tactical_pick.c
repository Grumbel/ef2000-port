/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F32(a) (*(float *)ef_guest_memory_ptr((ef_addr32)(a)))

#define PICK_TARGET_TYPE 0x1a54f4u   /* word, compared with the node type byte */
#define PICK_RADIUS 0x184c78u        /* float 6.0 */
#define MAP_ZOOM 0x1a53dfu           /* float */
#define MAP_HEIGHT 0x190u            /* 400 rows */

#define PICKED_FLAG 0x1a54f3u        /* byte */
#define PICKED_ACCUM 0x1a54f6u       /* dword */
#define PICKED_NODE 0x1a54fau        /* dword */
#define PICKED_X 0x1a54feu           /* word */
#define PICKED_Y 0x1a5500u           /* word */
#define PICKED_RANGE 0x1a5502u       /* dword */
#define PICKED_MODE 0x1a559au        /* byte */
#define CURRENT_MODE 0x1a559bu       /* byte */

/* True when COORD is within RADIUS of the cursor, with the original's
   comparison order (a NaN radius fails the first test only). */
static int near_cursor(float coord, float radius, float cursor)
{
	if (!((double)coord + (double)radius > (double)cursor))
		return 0;
	return !((double)coord - (double)radius >= (double)cursor);
}

/* 0x4c900, 397 bytes. Pick the first marker under the GUI cursor. GROUPS
   (EAX) is the group list for 0xcc1ac/0xcc1ec, LIST_HEAD (EDX) points at
   the first node; nodes link through +4. A node qualifies when its type
   byte +0xa equals the target type word, one of the two group-flag tests
   on its group byte +0xb holds, and its record (+0x13) map position, x
   word +8 and y = 400 - word +0xc, lies within 6.0 / zoom of the cursor
   on both axes. The picked node, position and range (the high word of the
   dword +0x55 of the record's object +0x20, times 100) are stored and the
   tactical display is refreshed. */
void pick_marker_4c900(ef_addr32 groups, ef_addr32 list_head)
{
	ef_addr32 node;

	for (node = EF_U32(list_head); node != 0; node = EF_U32(node + 4u)) {
		ef_addr32 rec;
		uint16_t x, y;
		float radius;

		if ((uint16_t)EF_U8(node + 0xau) != EF_U16(PICK_TARGET_TYPE))
			continue;
		if ((uint16_t)group_flag_negative_cc1ac(0, EF_U8(node + 0xbu), groups) == 0
		    && (uint16_t)group_flag_zero_cc1ec(0, EF_U8(node + 0xbu), groups) == 0)
			continue;
		radius = (float)((double)EF_F32(PICK_RADIUS) / (double)EF_F32(MAP_ZOOM));
		rec = EF_U32(node + 0x13u);
		x = EF_U16(rec + 8u);
		y = (uint16_t)(MAP_HEIGHT - EF_U16(rec + 0xcu));
		if (!near_cursor((float)x, radius, (float)(uint16_t)g_gui_cursor_x))
			continue;
		if (!near_cursor((float)y, radius, (float)(uint16_t)g_gui_cursor_y))
			continue;

		EF_U16(PICKED_X) = x;
		EF_U8(PICKED_FLAG) = 1;
		EF_U32(PICKED_NODE) = node;
		EF_U16(PICKED_Y) = y;
		EF_U32(PICKED_RANGE) = (uint32_t)((EF_S32(EF_U32(rec + 0x20u) + 0x55u) >> 16) * 100);
		EF_U32(PICKED_ACCUM) = 0;
		EF_U8(PICKED_MODE) = EF_U8(CURRENT_MODE);
		advance_first_tactical_status();
		reset_tactical_slots();
		fill_dialog_labels_4cff4();
		clear_pending_4ca90();
		return;
	}
}
