/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network packet 0x42: a grid record slot changed remotely. */

#define NET_SLOT_RECORD 0x1f71ccu      /* int16_t grid record the packet may name */
#define NET_SLOT_UNIT 0x1f71ceu        /* int16_t unit whose slot values are read */
#define NET_SLOT_VALUE_LO 0x12379cu    /* double, kept as two dwords */
#define NET_SLOT_VALUE_HI 0x1237a0u
#define NET_SLOT_FROZEN 0x1a363cu      /* uint8_t: keep the shown value */
#define NET_SLOT_SHOWN_LO 0x1a3108u
#define NET_SLOT_SHOWN_HI 0x1a310cu
#define UNIT_STRIDE 0xca
#define UNIT_SLOT 0x8e
#define NO_SLOT_VALUE_LO 0xcccccccdu   /* -304.8 */
#define NO_SLOT_VALUE_HI 0xc0730cccu

/* Exact +0.0/-0.0 test on the stored bits (the original tests the words). */
static int is_zero_bits(uint32_t lo, uint32_t hi)
{
	return (hi & 0x7fffffffu) == 0 && lo == 0;
}

static ef_addr32 record_slot(int16_t record)
{
	return g_world_grid_base + (int32_t)record * UNIT_STRIDE + UNIT_SLOT;
}

void net_packet_42_55bf8(ef_addr32 data)
{
	int16_t unit = EF_S16(NET_SLOT_UNIT);
	int16_t record;
	union { double d; uint32_t w[2]; } value;

	if (lookup_filtered_indexed_object(unit) == 0)
		return;
	record = EF_S16(NET_SLOT_RECORD);
	if (EF_S16(data + 1) != record)
		return;
	EF_U8(record_slot(record)) = EF_U8(data + 3);

	if (EF_U8(data + 5) != 0) {
		/* Only follow the change when the previous slot still shows the
		   remembered value (or none). */
		double previous = *(double *)ef_guest_memory_ptr(NET_SLOT_VALUE_LO);
		value.d = unit_slot_value_84f60(unit, (uint8_t)(EF_U8(record_slot(record)) - 1));
		if (!(value.d == previous) && !is_zero_bits(value.w[0], value.w[1]))
			return;
	}

	value.d = unit_slot_value_84f60(unit, EF_U8(record_slot(record)));
	if (is_zero_bits(value.w[0], value.w[1])) {
		EF_U32(NET_SLOT_VALUE_LO) = NO_SLOT_VALUE_LO;
		EF_U32(NET_SLOT_VALUE_HI) = NO_SLOT_VALUE_HI;
	} else {
		EF_U32(NET_SLOT_VALUE_LO) = value.w[0];
		EF_U32(NET_SLOT_VALUE_HI) = value.w[1];
	}
	if (EF_U8(NET_SLOT_FROZEN) == 0) {
		EF_U32(NET_SLOT_SHOWN_LO) = EF_U32(NET_SLOT_VALUE_LO);
		EF_U32(NET_SLOT_SHOWN_HI) = EF_U32(NET_SLOT_VALUE_HI);
	}
}
