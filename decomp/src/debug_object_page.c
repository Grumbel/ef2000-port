/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/*
 * Debug page 2 of the mission loop: the state of the unit under the
 * camera (target, threat, damage, refuelling, flight, base, radar, AI
 * mode and taxiing), drawn as text lines.
 */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define VIEW_KIND        EF_U8(0x0012db9eu)
#define VIEW_TARGET_UNIT EF_S16(0x001a823cu)  /* view kind 0x3e */
#define VIEW_PLAYER_UNIT EF_S16(0x001a8236u)  /* view kind 0x2d */
#define VIEW_FLIGHT      EF_S16(0x0019bb5eu)  /* view kind 0x2b, 0xff = none */
#define VIEW_MEMBER      EF_U8(0x0012dc34u)
#define NETWORK_GAME     EF_U8(0x00127da9u)
#define TOP_POINTER      EF_U32(0x00136b78u)
#define MACHINE_ID       EF_U16(0x001357aeu)
#define TAXI_SHOWN       EF_U8(0x001a3628u)
#define INDICATOR        0x001a3148u           /* double */

#define FLIGHTS          EF_U32(0x0019bb58u)   /* 0xca-byte records */
#define FLIGHT_SIZE      0xca
#define BASES            EF_U32(0x001972e0u)   /* 0x22-byte records */
#define BASE_SIZE        0x22
#define REFUEL_RECORDS   EF_U32(0x001f8918u)   /* 0x19-byte records */
#define REFUEL_SIZE      0x19
#define SQUADRONS        0x001252acu           /* 0x117-byte records */
#define SQUADRON_SIZE    0x117
#define SQUADRON_NAME    0x108

#define TEXT_SPACE       0x00183f88u           /* "" */
#define TEXT_THREAT      0x00183f8cu           /* "THREAT " */

/* Unit record fields; 16.16 values are read by their integer word. */
#define UNIT_NAME        0x2b
#define AI_MODE_TAXI     0x0b
#define AI_MODE_GROUND   0x06

#define U8_AT(base, offset)    EF_U8((base) + (offset))
#define U16_AT(base, offset)   EF_U16((base) + (offset))
#define U32_AT(base, offset)   EF_U32((base) + (offset))
#define S16_AT(base, offset)   EF_S16((base) + (offset))
#define INT_AT(base, offset)   EF_S16((base) + (offset) + 2u)
#define F64_AT(base, offset)   (*(double *)ef_guest_memory_ptr((ef_addr32)((base) + (offset))))
#define FLAG_AT(base, bit)     ((uint32_t)((U16_AT(base, 0x24) >> (bit)) & 1u))

static ef_addr32 flight_record(ef_addr32 unit)
{
	return FLIGHTS + (uint32_t)((int32_t)INT_AT(unit, 0x26) * FLIGHT_SIZE);
}

static ef_addr32 base_record(uint8_t base)
{
	return BASES + (uint32_t)base * BASE_SIZE;
}

/* Copies the unit's name in upper case into part, then appends it to text. */
static void append_unit_name(ef_addr32 text, ef_addr32 part, ef_addr32 unit)
{
	ef_addr32 name = unit + UNIT_NAME;

	if (name != 0)
		copy_string_9d749(part, name);
	else
		EF_U8(part) = EF_U8(TEXT_SPACE);
	uppercase_string(part);
	strcat_9ca57(text, part);
}

static ef_addr32 viewed_unit(void)
{
	ef_addr32 unit = 0;
	int32_t flight;

	if (VIEW_KIND == 0x3e)
		unit = guest_fn_7489c(VIEW_TARGET_UNIT);
	if (VIEW_KIND == 0x2d)
		unit = guest_fn_7489c(VIEW_PLAYER_UNIT);
	flight = VIEW_FLIGHT;
	if (flight != 0xff && VIEW_KIND == 0x2b)
		unit = guest_fn_7489c(INT_AT(FLIGHTS + (uint32_t)(flight * FLIGHT_SIZE),
				0x4eu + (uint32_t)VIEW_MEMBER * 2u));
	return unit;
}

void draw_unit_debug_page_3f7fc(void)
{
	char line[0x78];
	char part_text[0x64];
	ef_addr32 text = (ef_addr32)(uintptr_t)line;
	ef_addr32 part = (ef_addr32)(uintptr_t)part_text;
	ef_addr32 unit;
	ef_addr32 other;
	ef_addr32 flight;
	ef_addr32 waypoint;
	ef_addr32 base;
	ef_addr32 aprons;
	ef_addr32 refuel;
	ef_addr32 source;
	ef_addr32 target;
	uint8_t index;

	guest_sprintf_a4342(text, 0x00183f3cu, F64_AT(INDICATOR, 0));
	draw_shadowed_text_48c90(0, 0x14, 0x3c, text);
	unit = viewed_unit();
	if (NETWORK_GAME != 0) {
		guest_sprintf_a4342(text, 0x00183f4cu, (uint32_t)EF_U8(TOP_POINTER));
		draw_shadowed_text_48c90(0, 0x28, 0xc8, text);
	}
	if (unit == 0)
		return;

	/* "<id> <name> [WING <name>] TRACKING <range> <time> <target>" */
	guest_sprintf_a4342(text, 0x00183f58u, (int32_t)INT_AT(unit, 0x14));
	if (INT_AT(unit, 0x26) == 0xff) {
		copy_string_9d749(part, return_zero_8f3c0());
	} else if (unit + UNIT_NAME != 0) {
		copy_string_9d749(part, unit + UNIT_NAME);
	}
	uppercase_string(part);
	strcat_9ca57(text, part);
	other = guest_fn_7489c(INT_AT(unit, 0x41));
	if (other != 0) {
		strcat_9ca57(text, 0x00183f5cu);
		if (other + UNIT_NAME != 0)
			copy_string_9d749(part, other + UNIT_NAME);
		uppercase_string(part);
		strcat_9ca57(text, part);
	}
	strcat_9ca57(text, 0x00183f64u);
	guest_sprintf_a4342(part, 0x00183f70u, F64_AT(unit, 0x150), (uint32_t)U16_AT(unit, 0x146));
	strcat_9ca57(text, part);
	if (U8_AT(unit, 0x16c) == AI_MODE_GROUND) {
		guest_sprintf_a4342(part, 0x00183f7cu, (int32_t)INT_AT(unit, 0x168));
		strcat_9ca57(text, part);
	} else {
		target = guest_fn_7489c(INT_AT(unit, 0x144));
		if (target != 0)
			append_unit_name(text, part, target);
	}
	draw_shadowed_text_48c90(0, 0x14, 0, text);

	/* "THREAT <range> <time> <threat>" */
	source = TEXT_THREAT;
	for (index = 0; index < 8; index++)
		EF_U8(text + index) = EF_U8(source + index);
	guest_sprintf_a4342(part, 0x00183f70u, F64_AT(unit, 0x15a), (uint32_t)U16_AT(unit, 0x158));
	strcat_9ca57(text, part);
	target = guest_fn_7489c(INT_AT(unit, 0x156));
	if (target != 0)
		append_unit_name(text, part, target);
	draw_shadowed_text_48c90(0, 0x14, 0xa, text);

	guest_sprintf_a4342(text, 0x00183f94u,
			(uint32_t)U8_AT(unit, 0x18e), (uint32_t)U8_AT(unit, 0x18f),
			(uint32_t)U8_AT(unit, 0x190), (uint32_t)U8_AT(unit, 0x191),
			(uint32_t)U8_AT(unit, 0x192), (uint32_t)U8_AT(unit, 0x193),
			(uint32_t)U8_AT(unit, 0x194), (uint32_t)U8_AT(unit, 0x195),
			(uint32_t)U8_AT(unit, 0x196), (uint32_t)U8_AT(unit, 0x197),
			(uint32_t)U8_AT(unit, 0x198), (uint32_t)U8_AT(unit, 0x199));
	draw_prop_text_15a14(0, 0xa, 0x46, text);

	index = U8_AT(unit, 0x187);
	if (index != 0xff) {
		refuel = REFUEL_RECORDS + (uint32_t)index * REFUEL_SIZE;
		guest_sprintf_a4342(text, 0x00183fc0u, (uint32_t)index,
				(int32_t)INT_AT(refuel, 0x11), (int32_t)INT_AT(refuel, 0x13),
				(int32_t)INT_AT(refuel, 0x15), (int32_t)INT_AT(refuel, 0x01),
				(int32_t)INT_AT(refuel, 0x03), (int32_t)INT_AT(refuel, 0x05),
				(int32_t)INT_AT(refuel, 0x07), (int32_t)INT_AT(refuel, 0x09),
				(int32_t)INT_AT(refuel, 0x0b));
		draw_prop_text_15a14(0, 0xa, 0x50, text);
	}

	/* "FLIGHT ... WP x y T n RUN n <squadron> a b c d" */
	flight = flight_record(unit);
	waypoint = U32_AT(flight, 0x8a) + (uint32_t)U8_AT(flight, 0x8e) * 0x19u;
	guest_sprintf_a4342(text, 0x00183fecu, (int32_t)INT_AT(unit, 0x26),
			(uint32_t)U8_AT(flight, 0x3f), (uint32_t)U8_AT(flight, 0x4f),
			(uint32_t)U8_AT(flight, 0x1a), (uint32_t)U8_AT(flight, 0x5f),
			((int32_t)U32_AT(waypoint, 0) - 0x400) / 0x800,
			((int32_t)U32_AT(waypoint, 4) - 0x400) / 0x800,
			(uint32_t)U8_AT(waypoint, 0x18), (uint32_t)U8_AT(flight, 0x60));
	strcat_9ca57(text, SQUADRONS + (uint32_t)U8_AT(flight_record(unit), 0x5e) * SQUADRON_SIZE + SQUADRON_NAME);
	flight = flight_record(unit);
	guest_sprintf_a4342(part, 0x0018402cu,
			(uint32_t)U16_AT(flight, 0x50), (uint32_t)U16_AT(flight, 0x52),
			(uint32_t)U16_AT(flight, 0x54), (uint32_t)U16_AT(flight, 0x56));
	strcat_9ca57(text, part);
	draw_prop_text_15a14(0, 0xa, 0x5a, text);

	index = U8_AT(flight_record(unit), 0x5f);
	base = base_record(index);
	aprons = U32_AT(base, 0x1e);
	guest_sprintf_a4342(text, 0x0018403cu, (uint32_t)index,
			(uint32_t)U8_AT(base, 0x13), (uint32_t)U8_AT(base, 0x14),
			(int32_t)INT_AT(aprons, 0x0b), (int32_t)INT_AT(aprons, 0x24),
			(int32_t)INT_AT(aprons, 0x3d), (int32_t)INT_AT(aprons, 0x56));
	draw_prop_text_15a14(0, 1, 0x64, text);

	guest_sprintf_a4342(text, 0x00184064u,
			(int32_t)INT_AT(unit, 0x129), (int32_t)INT_AT(unit, 0x12b),
			(int32_t)INT_AT(unit, 0x12d), (int32_t)INT_AT(unit, 0x12f),
			(int32_t)INT_AT(unit, 0x131), (int32_t)INT_AT(unit, 0x133),
			(int32_t)INT_AT(unit, 0x135), (int32_t)INT_AT(unit, 0x137),
			(uint32_t)U8_AT(unit, 0x12a), (uint32_t)U8_AT(unit, 0x13e),
			(int32_t)INT_AT(unit, 0x139), (uint32_t)U8_AT(unit, 0x13d),
			(int32_t)INT_AT(unit, 0x141), (uint32_t)U8_AT(unit, 0x13f));
	draw_prop_text_15a14(0, 1, 0x6e, text);

	/* "DIE FREE CRP PROBE ATTCH REF MODE DF MODE- <dogfight mode> STAGE ..." */
	guest_sprintf_a4342(text, 0x001840a8u, FLAG_AT(unit, 2), FLAG_AT(unit, 3),
			FLAG_AT(unit, 1), FLAG_AT(unit, 10), (uint32_t)U8_AT(unit, 0x18a),
			(int32_t)INT_AT(unit, 0x186), (uint32_t)U8_AT(unit, 0x16c));
	switch (U8_AT(unit, 0x16d)) {
	case 0: guest_sprintf_a4342(part, 0x001840fcu); break;  /* BVR */
	case 1: guest_sprintf_a4342(part, 0x00184110u); break;  /* 2D */
	case 2: guest_sprintf_a4342(part, 0x00184114u); break;  /* 3D */
	case 3: guest_sprintf_a4342(part, 0x00184100u); break;  /* FREE FIGHTER */
	case 8: guest_sprintf_a4342(part, 0x001840ecu); break;  /* DODGE MISSILE */
	default:
		guest_sprintf_a4342(part, 0x00184118u, (uint32_t)U8_AT(unit, 0x16d));
		break;
	}
	strcat_9ca57(text, part);
	guest_sprintf_a4342(part, 0x0018411cu, (uint32_t)U8_AT(unit, 0x16e), U32_AT(unit, 0x1c),
			F64_AT(unit, 0x148));
	strcat_9ca57(text, part);
	draw_prop_text_15a14(0xc, 0xa, 0x78, text);

	guest_sprintf_a4342(text, 0x0018413cu,
			(uint32_t)U8_AT(unit, 0x209), (uint32_t)U8_AT(unit, 0x20a),
			(uint32_t)U8_AT(unit, 0x20b), (uint32_t)U8_AT(unit, 0x20c),
			(uint32_t)U8_AT(unit, 0x18b), (uint32_t)U8_AT(unit, 0x18c));
	draw_prop_text_15a14(0xfe, 0xa, 0x82, text);

	guest_sprintf_a4342(text, 0x00184174u, (uint32_t)MACHINE_ID, (int32_t)INT_AT(unit, 0x16),
			FLAG_AT(unit, 2), FLAG_AT(unit, 3), F64_AT(unit, 0x1dc), F64_AT(unit, 0x91),
			(uint32_t)U8_AT(unit, 0x18d), F64_AT(unit, 0x177));
	draw_prop_text_15a14(0xc, 0xa, 0xa0, text);

	if (U8_AT(unit, 0x16c) != AI_MODE_TAXI)
		return;
	if (U8_AT(unit, 0x209) != 0xff)
		draw_outline_segments_3d7b0(U8_AT(flight_record(unit), 0x5f), U8_AT(unit, 0x209), 6);
	if (TAXI_SHOWN >= U8_AT(base_record(U8_AT(flight_record(unit), 0x5f)), 0x14))
		TAXI_SHOWN = 0;
	draw_outline_segments_3d7b0(U8_AT(flight_record(unit), 0x5f), TAXI_SHOWN, 0xf);
	guest_sprintf_a4342(text, 0x001841c4u, (uint32_t)TAXI_SHOWN);
	draw_prop_text_15a14(0xc, 0x96, 0xbe, text);
}
