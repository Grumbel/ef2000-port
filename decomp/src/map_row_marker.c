/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#define ROW_STRIDE   0x45
#define ROW_X        0x07   /* int32, 1/2048 map pixels */
#define ROW_Y        0x0f   /* int32, 1/2048 map pixels, north up */
#define ROW_HEADING  0x14   /* int16 */
#define ROW_COMMAND  0x1c   /* uint8 command code */
#define ROW_SIDE     0x1d   /* int16 */

#define MAP_SIZE     400

#define HEADING_SCALE EF_F64(0x0018448au)
#define HEADING_MUL_A EF_F64(0x00184492u)
#define HEADING_MUL_B EF_F64(0x0018449au)

enum {
	COLOR_IDLE = 4,
	COLOR_COMMAND = 5,
	COLOR_COMMAND_OTHER = 6,
	IDLE_FLAGS = 0x1fff
};

static int16_t clamp_map(int16_t value)
{
	if (value < 0)
		value = 0;
	if (value > MAP_SIZE - 1)
		value = MAP_SIZE - 1;
	return value;
}

/* Draw the campaign-map marker for network row ROW.  The marker colour shows
 * whether the row carries a group command; its symbol shows the side. */
void draw_map_row_41b98(int16_t row)
{
	ef_addr32 rec = g_external_grid_base + (uint32_t)((int32_t)row * ROW_STRIDE);
	int16_t x = clamp_map((int16_t)(EF_S32(rec + ROW_X) >> 11));
	int16_t y = clamp_map((int16_t)(MAP_SIZE - (EF_S32(rec + ROW_Y) >> 11)));
	int16_t symbol;
	int32_t color;
	uint32_t flags;

	/* ORIGINAL: the heading is converted and then never used. */
	(void)truncate_9d72c(((double)EF_S16(rec + ROW_HEADING) * HEADING_SCALE + 1.0)
		* HEADING_MUL_A * HEADING_MUL_B);

	if (command_code_is_group_1b(EF_U8(rec + ROW_COMMAND)) != 0u) {
		color = code_is_26_or_27(EF_U8(rec + ROW_COMMAND)) == 0u
			? COLOR_COMMAND_OTHER : COLOR_COMMAND;
		flags = 0;
	} else {
		color = COLOR_IDLE;
		flags = IDLE_FLAGS;
	}

	switch (EF_S16(rec + ROW_SIDE)) {
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
	guest_fn_4110c(color, symbol, x, y, flags);
}
