/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define UNIT_STRIDE 0xca
#define CONTACT_RANGE_SCALE 0x186b24u /* 0.000539: world units to nautical miles */
#define CONTACT_ENTRY 0x2c
#define CONTACT_CHANCE 80

static uint16_t unit_side(ef_addr32 object)
{
	return EF_U16(g_world_grid_base + (int32_t)EF_S16(object + 0x28) * UNIT_STRIDE + 0x15);
}

/* Occasionally (one step in 80) report a contact between object
   `index` (kinds 1, 3 and 4) and an object `other` of another side,
   with its bearing and range. */
void report_contact_73820(int16_t index, int16_t other)
{
	ef_addr32 object = lookup_filtered_indexed_object(index);
	ef_addr32 target = lookup_filtered_indexed_object(other);
	uint8_t kind;
	uint8_t level;
	int32_t range;
	int16_t bearing;

	if (object == 0 || target == 0)
		return;
	kind = EF_U8(object + 0x16c);
	if (kind != 1 && kind != 3 && kind != 4)
		return;
	if (unit_side(target) == unit_side(object))
		return;
	if ((uint16_t)step_generator_a45b3() % CONTACT_CHANCE != 0)
		return;

	level = scale_to_byte_4600c(EF_F64(target + 0x59));
	range = (int32_t)truncate_9d72c(indexed_object_distance(index, other)
	                                * EF_F64(CONTACT_RANGE_SCALE));
	bearing = (int16_t)scale_to_angle_45fc4(unit_offset_value_6a5ac(object + 0x49, other));
	draw_object_entries_46168((uint32_t)(int32_t)index, CONTACT_ENTRY,
	                          (uint32_t)(int32_t)bearing, (uint32_t)range, level);
}
