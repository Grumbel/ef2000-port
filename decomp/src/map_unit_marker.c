/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define UNIT_STRIDE      0xca
#define UNIT_SIDE        0x15   /* int16 */
#define UNIT_TYPE        0x1b   /* uint8 */
#define UNIT_Y           0x61   /* double */
#define UNIT_X           0x69   /* double */
#define UNIT_VISIBILITY  0x79   /* double */
#define UNIT_HEADING     0x81   /* double */

#define HEADING_SCALE    EF_F64(0x0018446au)
#define HEADING_MUL_A    EF_F64(0x00184472u)
#define HEADING_MUL_B    EF_F64(0x0018447au)
#define VISIBLE_MIN      EF_F64(0x00184482u)

#define PREVIEW_NAME     0x001a49d8u  /* current preview picture name */
#define MAP_PICTURE_NAME 0x00184424u

#define MAP_SIZE         400
#define HIGHLIGHT_SIZE   12

static int16_t clamp_map(int16_t value)
{
	if (value < 0)
		value = 0;
	if (value > MAP_SIZE - 1)
		value = MAP_SIZE - 1;
	return value;
}

/* Draw the campaign-map marker for unit UNIT: its symbol shows the side and
 * its colour the unit type, rotated to the unit's heading.  Units below the
 * visibility threshold are only drawn when selected; the selected unit also
 * gets a highlight box. */
void draw_map_unit_41998(int16_t unit)
{
	ef_addr32 rec = g_world_grid_base + (uint32_t)((int32_t)unit * UNIT_STRIDE);
	int16_t x = clamp_map((int16_t)double_to_scaled_b4250(EF_F64(rec + UNIT_X)));
	int16_t y = clamp_map((int16_t)(MAP_SIZE - double_to_scaled_b4250(EF_F64(rec + UNIT_Y))));
	int16_t heading;
	int16_t symbol;
	int32_t color;
	uint8_t type;

	heading = (int16_t)(int32_t)truncate_9d72c(
		(EF_F64(rec + UNIT_HEADING) * HEADING_SCALE + 1.0)
		* HEADING_MUL_A * HEADING_MUL_B);
	if (heading < 0x7fff)
		heading = (int16_t)(heading + 0x7fff);
	else
		heading = (int16_t)(heading - 0x7fff);

	if (!(EF_F64(rec + UNIT_VISIBILITY) > VISIBLE_MIN)
	    && unit != g_adjacency_selected_value)
		return;

	switch (EF_S16(rec + UNIT_SIDE)) {
	case 1:
		symbol = 3;
		break;
	case 2:
		symbol = 5;
		break;
	default:
		symbol = 1;
		break;
	}

	type = EF_U8(rec + UNIT_TYPE);
	if (type == 4 || type == 8)
		color = 2;
	else if (type == 0)
		color = 3;
	else
		color = 1;
	guest_fn_4110c(color, symbol, x, y, (uint32_t)(int32_t)heading);

	if (unit == g_adjacency_selected_value) {
		color = guest_strcmp(PREVIEW_NAME, MAP_PICTURE_NAME) != 0 ? 0 : 0xf;
		guest_fn_c9cac(color, x, y, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE);
	}
}
