/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_S32(a) (*(int32_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#define CURRENT_OBJECT      EF_U32(0x001f71bcu)
#define TARGET_RANGE        EF_F64(0x001f7000u)
#define TARGET_ANGLE_OFF    EF_F64(0x001f7018u)
#define RANGE_SCALE         EF_F64(0x00182444u)
#define GUN_MAX_RANGE       EF_F64(0x00182454u)
#define MISSILE_MAX_ANGLE   EF_F64(0x0018244cu)
#define BOMB_MAX_ANGLE      EF_F64(0x0018243cu)
#define TYPE1_MAX_ANGLE     EF_F64(0x0018245cu)

#define WEAPON_RECORD_SIZE  145
#define WEAPON_MAX_RANGE    0x0012bca3u   /* int32 per weapon record */

#define OBJ_TARGET          0x168   /* target unit in the high word */
#define OBJ_MODE            0x16c

enum { MODE_LOS_CHECK = 6 };

static int16_t current_target(void)
{
	return (int16_t)(EF_S32(CURRENT_OBJECT + OBJ_TARGET) >> 16);
}

/* Missiles need the target inside the angle limit and an unlinked target. */
static uint8_t missile_usable(void)
{
	if (abs_double_b427c(TARGET_ANGLE_OFF) > MISSILE_MAX_ANGLE)
		return 0;
	if (object_has_link(current_target()) != 0)
		return 0;
	return 1;
}

/* 0x291fc, 435 bytes.  AL weapon type -> AL nonzero when the current
 * object may fire weapon TYPE at its target now: never while it is in
 * mode 6 with line of sight to the target, otherwise according to the
 * range and angle-off limits of the weapon type.  Preserves all
 * registers. */
uint8_t weapon_usable_291fc(uint8_t type)
{
	ef_addr32 obj = CURRENT_OBJECT;

	if (EF_U8(obj + OBJ_MODE) == MODE_LOS_CHECK
	    && request_los_66cd4((int16_t)(EF_S32(obj + OBJ_TARGET) >> 16), obj) != 0)
		return 0;

	switch (type) {
	case 8:
		if (!((double)EF_S32(WEAPON_MAX_RANGE + (uint32_t)type * WEAPON_RECORD_SIZE)
		      * RANGE_SCALE >= TARGET_RANGE))
			return 0;
		return missile_usable();
	case 6:
		if (TARGET_RANGE > GUN_MAX_RANGE)
			return 0;
		return missile_usable();
	case 7:
	case 0x1c:
		return missile_usable();
	case 9:
	case 0x14:
		return abs_double_b427c(TARGET_ANGLE_OFF) > BOMB_MAX_ANGLE ? 0 : 1;
	case 1:
		return abs_double_b427c(TARGET_ANGLE_OFF) > TYPE1_MAX_ANGLE ? 0 : 1;
	default:
		return 1;
	}
}
