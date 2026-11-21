/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* List node fields. */
#define NODE_NEXT 0x4
#define NODE_KIND 0xa
#define NODE_KEY 0xe
#define NODE_ENTRY_COUNT 0x12
#define NODE_GROUP 0x13 /* pointer to the entry array */
#define KIND_GROUP 0x10
#define KEY_FIELD 0x9

/* Group entries, 0x7c bytes each; the first holds the busy word. */
#define ENTRY_SIZE 0x7c
#define ENTRY_KIND 0x2
#define ENTRY_BUSY 0x1e
#define ENTRY_OBJECT 0x20
#define KIND_TRACKED 6

/* Tracked object fields. */
#define OBJECT_POS 0x4b /* two dwords */
#define OBJECT_HEADING 0x57

/* Owner record: type byte at +0, slot array (0x19 bytes per slot) at +0x20. */
#define OWNER_SLOTS 0x20
#define SLOT_SIZE 0x19
#define SLOT_STATE_TRACKING 5

#define TYPE_TABLE_SPEED 0x128591u /* double, 0xe5 bytes per type */
#define TYPE_RECORD_SIZE 0xe5
#define SPEED_SCALE 0x18e8f0u      /* 1.942 */

/* For every idle group of the list at *head whose key matches `key`, fill
   slot `slot` of the owner from each of its tracked entries (the last one
   wins): object position and heading, cleared offsets, state 5 and the
   owner type's scaled speed. */
void fill_group_slot_b513c(ef_addr32 owner, int16_t slot, ef_addr32 key, ef_addr32 head)
{
	ef_addr32 node;

	for (node = EF_U32(head); node != 0; node = EF_U32(node + NODE_NEXT)) {
		ef_addr32 group = EF_U32(node + NODE_GROUP);
		int16_t i;

		if (EF_U16(node + NODE_KEY) != EF_U16(key + KEY_FIELD) || EF_U8(node + NODE_KIND) != KIND_GROUP ||
		    EF_U16(group + ENTRY_BUSY) != 0)
			continue;
		for (i = 0; i < (int16_t)EF_U8(node + NODE_ENTRY_COUNT); ++i) {
			ef_addr32 entry = EF_U32(node + NODE_GROUP) + (uint32_t)((int32_t)i * ENTRY_SIZE);
			ef_addr32 object = EF_U32(entry + ENTRY_OBJECT);
			ef_addr32 out;

			if (EF_U8(entry + ENTRY_KIND) != KIND_TRACKED || object == 0)
				continue;
			out = EF_U32(owner + OWNER_SLOTS) + (uint32_t)((int32_t)slot * SLOT_SIZE);
			EF_U32(out) = EF_U32(object + OBJECT_POS);
			EF_U32(out + 4) = EF_U32(object + OBJECT_POS + 4);
			EF_U16(out + 0xc) = EF_U16(object + OBJECT_HEADING);
			EF_U16(out + 0x8) = 0;
			EF_U16(out + 0xa) = 0;
			EF_U8(out + 0x18) = SLOT_STATE_TRACKING;
			EF_U16(out + 0xe) = 0;
			EF_U16(out + 0x10) = 0;
			EF_U16(out + 0x12) = 0;
			EF_U16(out + 0x14) = 0;
			EF_U16(out + 0x16) = (uint16_t)(int32_t)truncate_9d72c(
				EF_F64(TYPE_TABLE_SPEED + (uint32_t)EF_U8(owner) * TYPE_RECORD_SIZE) * EF_F64(SPEED_SCALE));
		}
	}
}
