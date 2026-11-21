/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network packet 0x0b: the full state of a remote object. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define NET_SENDER 0x1a6668u
#define NET_SLOTS 0x135878u         /* 32-byte slots, +0 header pointer */
#define NET_KEEP_OWNER 0x1f9012u    /* uint8_t: do not take over ownership */
#define OWNER_TRACKS 0x1a5c30u      /* 27-byte entries per owner */
#define OWNER_TRACK_SIZE 27
#define OBJECT_FLAGS 0x24           /* flag byte pairs at +0x24 and +0x26 */
#define OBJECT_OWNER 0x18

#define SPEED_WORD_SCALE 0x185bf4u  /* 2^-16 */
#define SPEED_SCALE 0x185bfcu       /* 0.515 */
#define SIGNED_BYTE_SCALE 0x185c04u /* 1/127 */

#define BIT(value, n) (((value) >> (n)) & 1u)

static void set_bit(ef_addr32 address, unsigned n, unsigned value)
{
	EF_U8(address) = (uint8_t)((EF_U8(address) & ~(1u << n)) | (value << n));
}

void net_packet_0b_55144(ef_addr32 data, uint32_t slot)
{
	ef_addr32 object = lookup_filtered_indexed_object(EF_S16(data + 0x1b));
	ef_addr32 track;
	double scale;
	uint8_t flags;
	uint8_t state;

	if (object == 0)
		return;
	flags = EF_U8(data + 0x17);
	state = EF_U8(data + 0x19);
	if ((EF_U8(object + OBJECT_FLAGS) & 8) == 0)
		set_bit(object + OBJECT_FLAGS, 3, BIT(flags, 2));
	if (EF_U16(object + OBJECT_OWNER) == (uint16_t)g_world_grid_marker)
		return;
	if (EF_S16(object + OBJECT_OWNER) == 0xff)
		return;

	EF_U32(object + 0x00) = EF_U32(data + 0x01);
	EF_U32(object + 0x04) = EF_U32(data + 0x05);
	EF_U32(object + 0x08) = EF_U32(data + 0x09);
	EF_U16(object + 0x0c) = EF_U16(data + 0x0d);
	EF_U16(object + 0x0e) = EF_U16(data + 0x0f);
	EF_U16(object + 0x10) = EF_U16(data + 0x11);
	set_bit(object + 0x24, 2, BIT(flags, 1));
	set_bit(object + 0x24, 1, BIT(flags, 0));
	set_bit(object + 0x26, 5, BIT(flags, 3));
	set_bit(object + 0x26, 6, BIT(flags, 4));
	set_bit(object + 0x26, 2, BIT(flags, 5));
	set_bit(object + 0x24, 0, BIT(flags, 6));

	EF_U32(object + 0x12) = EF_U32(data + 0x13);
	EF_F64(object + 0x91) = (double)EF_S32(object + 0x12)
		* EF_F64(SPEED_WORD_SCALE) * EF_F64(SPEED_SCALE);
	guest_fn_73ee0_x(EF_S16(object + 0x16));
	EF_U16(object + 0x234) = EF_U16(EF_U32(NET_SLOTS + (int32_t)(int16_t)slot * 32) + 0x0b);
	EF_U16(object + 0x236) = 0;
	advance_object_position_724a8((uint32_t)(int32_t)EF_S16(object + 0x16), object);

	EF_U16(object + 0x146) = EF_U16(data + 0x1d);
	scale = EF_F64(SIGNED_BYTE_SCALE);
	EF_F64(object + 0x150) = scale * (double)(int8_t)EF_U8(data + 0x1f);
	EF_U16(object + 0x158) = EF_U16(data + 0x20);
	EF_F64(object + 0x15a) = scale * (double)(int8_t)EF_U8(data + 0x22);

	set_bit(object + 0x24, 6, BIT(state, 0));
	set_bit(object + 0x25, 2, BIT(state, 1));
	EF_U16(object + 0x45) = (uint16_t)(EF_U8(data + 0x18) >> 3);
	EF_U8(object + 0x18a) = (uint8_t)BIT(state, 4);
	EF_U16(object + 0x188) = EF_U16(data + 0x28);
	EF_U8(object + 0x18b) = (uint8_t)((state >> 5) & 3u);
	EF_U8(object + 0x18c) = (uint8_t)(EF_U8(data + 0x1a) & 7u);
	EF_U8(object + 0x18d) = (uint8_t)BIT(state, 2);
	EF_U8(object + 0x141) = (uint8_t)BIT(state, 3);
	EF_U8(object + 0x16c) = EF_U8(data + 0x23);
	EF_F64(object + 0x177) = (double)EF_S16(data + 0x24);
	if (EF_S16(data + 0x26) == -1)
		EF_U32(object + 0x1c) = 0;
	if (EF_U8(NET_KEEP_OWNER) == 0)
		EF_U16(object + OBJECT_OWNER) = EF_U8(NET_SENDER);

	if ((EF_U8(data + 0x18) & 7) == 0)
		return;
	/* Remember the owner's view of this object: id and three doubles. */
	track = OWNER_TRACKS + (int32_t)EF_S16(object + OBJECT_OWNER) * OWNER_TRACK_SIZE;
	EF_U16(track) = EF_U16(data + 0x1b);
	EF_U32(track + 0x03) = EF_U32(object + 0x49);
	EF_U32(track + 0x07) = EF_U32(object + 0x4d);
	EF_U32(track + 0x0b) = EF_U32(object + 0x51);
	EF_U32(track + 0x0f) = EF_U32(object + 0x55);
	EF_U32(track + 0x13) = EF_U32(object + 0x59);
	EF_U32(track + 0x17) = EF_U32(object + 0x5d);
}
