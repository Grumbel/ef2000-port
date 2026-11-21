/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <string.h>

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define PLAYER_UNIT ((int32_t)EF_S32(0x001f71c8u) >> 16)
#define PLAYER_UNIT_WORD 0x1f71cau

/* Moving object fields. */
#define OBJ_TYPE 0x18
#define OBJ_ID_HIGH 0x50  /* id = (byte << 12) + word */
#define OBJ_ID_LOW 0x51
#define OBJ_TARGET 0x54   /* target unit in the high half */
#define OBJ_TARGET_UNIT 0x56
#define OBJ_SILENT 0x59
#define OBJ_KEEP_LINK 0x5a
#define NO_TARGET 0xff

/* Unit fields. */
#define UNIT_ALIVE 0x1c
#define UNIT_POS 0x49 /* x, y, z doubles */

#define TYPE_TABLE 0x12bc94u /* g_grid_category_types, 0x91 bytes per type */
#define TYPE_RECORD_SIZE 0x91
#define TYPE_CATEGORY 0
#define TYPE_HIT_KIND 3
#define PLAYER_EYE_OFFSET 0x18a44eu /* -3.0 */

#define HIT_REPORTED 0x12d7aau
#define HIT_KIND2 0x12d7abu
#define HIT_KIND1 0x12d7acu

static uint8_t type_byte(ef_addr32 object, uint32_t field)
{
	return EF_U8(TYPE_TABLE + (uint32_t)EF_U8(object + OBJ_TYPE) * TYPE_RECORD_SIZE + field);
}

static int16_t object_id(ef_addr32 object)
{
	return (int16_t)(((uint32_t)EF_U8(object + OBJ_ID_HIGH) << 12) + EF_U16(object + OBJ_ID_LOW));
}

/* Per-step contact checks of a moving object: drop its link to a target
   unit that is no longer alive (categories 1 and 3 except type 1), and,
   when it targets the living player, test it against the player
   position (lowered by 3) and report the hit by the type's hit kind. */
void check_object_contact_88134(ef_addr32 object)
{
	ef_addr32 player = guest_fn_7489c(PLAYER_UNIT);
	int32_t target = EF_S32(object + OBJ_TARGET) >> 16;
	double scene[3], eye[3];

	if (target != NO_TARGET && EF_U8(object + OBJ_KEEP_LINK) == 0) {
		ef_addr32 unit = guest_fn_7489c(target);
		if (unit != 0 && EF_U32(unit + UNIT_ALIVE) == 0) {
			uint8_t category = type_byte(object, TYPE_CATEGORY);
			if ((category == 1 || category == 3) && EF_U8(object + OBJ_TYPE) != 1)
				update_object_remove_link(unit, object);
		}
	}

	if (player == 0 || EF_U32(player + UNIT_ALIVE) == 0 ||
	    EF_U16(PLAYER_UNIT_WORD) != EF_U16(object + OBJ_TARGET_UNIT))
		return;

	convert_scene_vector(object, (ef_addr32)(uintptr_t)scene);
	memcpy(eye, ef_guest_memory_ptr(player + UNIT_POS), 2 * sizeof(double));
	eye[2] = EF_F64(player + UNIT_POS + 16) + EF_F64(PLAYER_EYE_OFFSET);
	if ((uint8_t)store_scaled_vectors_7b08c((uint32_t)(uintptr_t)eye, (uint32_t)(uintptr_t)scene) != 0)
		return;

	if (EF_U8(object + OBJ_SILENT) == 0)
		EF_U8(HIT_REPORTED) = 1;
	switch (type_byte(object, TYPE_HIT_KIND)) {
	case 1:
		EF_U8(HIT_KIND1) = 1;
		guest_fn_65030(3, object_id(object), 0);
		break;
	case 2:
		EF_U8(HIT_KIND2) = 1;
		guest_fn_65030(4, object_id(object), 0);
		break;
	}
}
