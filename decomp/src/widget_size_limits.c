/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Widget fields (words). */
#define WIDGET_FLAGS 0x3a
#define WIDGET_NO_SCREEN_CLAMP 0x10
#define WIDGET_WIDTH 0x45
#define WIDGET_HEIGHT 0x47
#define WIDGET_MIN_WIDTH 0x49
#define WIDGET_MIN_HEIGHT 0x4b
#define WIDGET_PAD_TOP 0x5d
#define WIDGET_PAD_BOTTOM 0x5f
#define WIDGET_PAD_LEFT 0x61
#define WIDGET_PAD_RIGHT 0x63
#define WIDGET_COLUMNS 0x65
#define WIDGET_ROWS 0x67
#define WIDGET_CELL_SCALE 0x9f /* double */

#define SCREEN_MAX_WIDTH 0x15b744u
#define SCREEN_MAX_HEIGHT 0x15b748u

/* Raise a size word to its minimum. The original compares the signed
   size with the unsigned minimum in 32 bits; the explicit sign test keeps
   Watcom from narrowing that to a 16-bit compare. */
static void raise_to_minimum(ef_addr32 size, ef_addr32 minimum)
{
	if ((int16_t)EF_U16(size) < 0 || EF_U16(size) < EF_U16(minimum))
		EF_U16(size) = EF_U16(minimum);
}

/* Lower a size word to cells * scale + both paddings when it exceeds it. */
static void lower_to_content(ef_addr32 widget, ef_addr32 size, uint32_t cells, uint32_t pad_a, uint32_t pad_b)
{
	double limit = (double)EF_U16(widget + cells) * EF_F64(widget + WIDGET_CELL_SCALE) +
	               (double)EF_U16(widget + pad_a) + (double)EF_U16(widget + pad_b);
	if ((double)EF_S16(size) > limit)
		EF_U16(size) = (uint16_t)(int32_t)truncate_9d72c(limit);
}

/* Keep a widget's size between its minimum, the size of its content and,
   unless flag 0x10 is set, the screen limits. */
void limit_widget_size_a5070(ef_addr32 widget)
{
	raise_to_minimum(widget + WIDGET_WIDTH, widget + WIDGET_MIN_WIDTH);
	raise_to_minimum(widget + WIDGET_HEIGHT, widget + WIDGET_MIN_HEIGHT);
	lower_to_content(widget, widget + WIDGET_WIDTH, WIDGET_COLUMNS, WIDGET_PAD_LEFT, WIDGET_PAD_RIGHT);
	lower_to_content(widget, widget + WIDGET_HEIGHT, WIDGET_ROWS, WIDGET_PAD_TOP, WIDGET_PAD_BOTTOM);

	if ((EF_U8(widget + WIDGET_FLAGS) & WIDGET_NO_SCREEN_CLAMP) == 0) {
		if ((int32_t)EF_S16(widget + WIDGET_WIDTH) > EF_S32(SCREEN_MAX_WIDTH))
			EF_U16(widget + WIDGET_WIDTH) = EF_U16(SCREEN_MAX_WIDTH);
		if ((int32_t)EF_S16(widget + WIDGET_HEIGHT) > EF_S32(SCREEN_MAX_HEIGHT))
			EF_U16(widget + WIDGET_HEIGHT) = EF_U16(SCREEN_MAX_HEIGHT);
	}
}
