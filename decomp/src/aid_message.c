/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Aid.c: choose the next in-flight aid message from the rule tables that
 * init_aid_data_tables_f80 prepares and the request block filled by 0x2e04. */

#include "ef_abi.h"
#include "ef_mem.h"

/* Request block: the objects the current situation refers to. */
#define AID_PLAYER        0x00197061u /* own aircraft */
#define AID_WEAPON        0x00197065u
#define AID_TARGET        0x00197069u
#define AID_GROUND_TARGET 0x0019706du
#define AID_THREAT        0x00197071u
#define AID_THREAT_TYPE   0x00197075u
#define AID_WINGMAN       0x00197079u
#define AID_SET           0x00197085u /* rule set, 0xff selects the alternative table */
#define AID_ROW           0x00197086u
#define AID_ALT_SET       0x00197087u

/* Chosen message. */
#define AID_PENDING       0x00196fecu /* word */
#define AID_SUBJECT       0x00196ff3u
#define AID_OBJECT        0x00196ff7u
#define AID_SUBJECT_TEXT  0x00196ffbu
#define AID_OBJECT_TEXT   0x0019702du
#define AID_KIND          0x0019707du
#define AID_OBJECT_KIND   0x0019707eu
#define AID_RULE          0x00197088u
#define AID_PARAM         0x00197089u /* word */

#define AID_RULES         0x00196398u /* 50 records of 0x3e bytes */
#define AID_RULE_SIZE     0x3eu
#define RULE_KIND         0x32u
#define RULE_OBJECT_KIND  0x33u
#define RULE_PARAM        0x34u
#define AID_RULE_ROWS     0x001961f9u /* 8 sets of 0x33 bytes, 5 rows of 10 rules */
#define AID_ALT_ROWS      0x00196192u /* 2 sets of 0x34 bytes */
#define AID_RULES_PER_ROW 10

#define OBJECT_TYPE       0x18u /* byte, index into the 0x91-byte type records */
#define OBJECT_ID         0x16u /* word */
#define OBJECT_NAME       0xe9u /* byte, index into the name table */
#define TYPE_RECORD_SIZE  0x91u
#define TYPE_NAMES        0x0012bc68u /* dword name pointer in each type record */
#define TYPE_CLASSES      0x0012bc94u /* byte class: 1 weapon-on-target, 2 weapon-on-ground */
#define OBJECT_NAMES      0x00132d34u
#define CALLSIGN_COUNT    0x001357b0u /* int16 */
#define CALLSIGN_IDS      0x001a5c30u /* word, 0x1b-byte stride */
#define CALLSIGN_NAMES    0x001a62f0u /* 0x11-byte strings */
#define PLAYER_ID         0x001f71ceu /* word */
#define NETWORK_GAME      0x00127da9u
#define PARACHUTE_AID     0x00120d30u

#define STR_S             0x0017f5d8u /* "%s" */
#define STR_S_PAREN       0x0017f5dcu /* "%s (%s)" */
#define STR_DASH_S        0x0017f5e4u /* "- %s" */
#define STR_DASH_S_PAREN  0x0017f5ecu /* "- %s (%s)" */
#define STR_EMPTY         0x0017f5f8u
#define STR_AID_FIXED     0x0017f5fcu /* "AID_FIXED" */
#define STR_AID_PARA      0x0017f608u /* "AID_PARA_FIXED" */
#define STR_BAILED_PILOT  0x001320a2u /* "ABGESPRUNGENER PILOT" */
#define STR_WEAPON        0x001320b7u /* "WAFFE" */
#define STR_TARGET        0x001320bdu /* "ZIEL" */
#define STR_WINGMAN       0x001320c2u /* "FLUEGELMANN" */
#define STR_THREAT        0x001320cdu /* "BEDROHUNG" */
#define STR_TARGET2       0x0012dfa4u /* "ZIEL" */
#define STR_GROUND_TARGET 0x0012fa20u /* "BODENZIEL" */
#define STR_PLAYER        0x0012ff09u /* "SPIELER" */

#define TEXT_SIZE 0x34u

static ef_addr32 obj(ef_addr32 slot)
{
	return EF_U32(slot);
}

static uint32_t type_index(ef_addr32 object)
{
	return EF_U8(object + OBJECT_TYPE);
}

static ef_addr32 type_name(ef_addr32 object)
{
	return EF_U32(TYPE_NAMES + type_index(object) * TYPE_RECORD_SIZE);
}

static ef_addr32 object_name(ef_addr32 object)
{
	return EF_U32(OBJECT_NAMES + EF_U8(object + OBJECT_NAME) * 4u);
}

/* Distance between two objects; z, y and x differences in that order. */
static int32_t distance(ef_addr32 a, ef_addr32 b)
{
	uint32_t z = abs_a28f2((int32_t)(EF_U32(a + 8u) - EF_U32(b + 8u)));
	uint32_t y = abs_a28f2((int32_t)(EF_U32(a + 4u) - EF_U32(b + 4u)));
	uint32_t x = abs_a28f2((int32_t)(EF_U32(a) - EF_U32(b)));

	return vector_length_3c74((int32_t)x, (int32_t)y, (int32_t)z);
}

static int32_t slot_distance(ef_addr32 slot)
{
	return obj(slot) != 0u ? distance(obj(AID_PLAYER), obj(slot)) : 0;
}

/* Replace TEXT with the call sign of the object in SLOT: in a network game
 * the matching entry of the call-sign table (last match wins), otherwise
 * "SPIELER" when it is the player's own aircraft. */
static void use_callsign(ef_addr32 text, ef_addr32 slot)
{
	if (EF_U8(NETWORK_GAME) != 0u) {
		int16_t i;

		if ((int16_t)EF_U16(CALLSIGN_COUNT) <= 0)
			return;
		i = 0;
		do {
			if (EF_U16(obj(slot) + OBJECT_ID) == EF_U16(CALLSIGN_IDS + (int32_t)i * 0x1b))
				guest_sprintf_a4342(text, STR_S, CALLSIGN_NAMES + (int32_t)i * 0x11);
			++i;
		} while (i < (int16_t)EF_U16(CALLSIGN_COUNT));
	} else if (EF_U16(PLAYER_ID) == EF_U16(obj(slot) + OBJECT_ID)) {
		guest_sprintf_a4342(text, STR_S, STR_PLAYER);
	}
}

static void named(ef_addr32 text, ef_addr32 slot)
{
	guest_sprintf_a4342(text, STR_S, object_name(obj(slot)));
	use_callsign(text, slot);
}

/* 0x20ec, 3211 bytes. Walk the ten rules of the current row and take the
 * first that is unconditional (kind 1, object kind 0) or whose schedule
 * allows it for one of the present objects at its distance. Fill the
 * subject/object texts and parameters of the chosen rule and announce it;
 * without a match fall back to "AID_FIXED". */
void select_aid_message_20ec(void)
{
	char text[TEXT_SIZE];
	ef_addr32 buf = (ef_addr32)(uintptr_t)text;
	int32_t weapon = slot_distance(AID_WEAPON);
	int32_t target = slot_distance(AID_TARGET);
	int32_t ground = slot_distance(AID_GROUND_TARGET);
	int32_t wingman = slot_distance(AID_WINGMAN);
	int32_t threat = slot_distance(AID_THREAT);
	int32_t threat_type = slot_distance(AID_THREAT_TYPE);
	int32_t weapon_target = 0;
	int32_t weapon_ground = 0;
	uint16_t found = 0u;
	uint16_t rule = 0u;
	ef_addr32 rec;
	int16_t i;

	if (obj(AID_WEAPON) != 0u && obj(AID_TARGET) != 0u &&
	    EF_U8(TYPE_CLASSES + type_index(obj(AID_WEAPON)) * TYPE_RECORD_SIZE) == 1u)
		weapon_target = distance(obj(AID_WEAPON), obj(AID_TARGET));
	if (obj(AID_WEAPON) != 0u && obj(AID_GROUND_TARGET) != 0u &&
	    EF_U8(TYPE_CLASSES + type_index(obj(AID_WEAPON)) * TYPE_RECORD_SIZE) == 2u)
		weapon_ground = distance(obj(AID_WEAPON), obj(AID_GROUND_TARGET));

	for (i = 0; i < AID_RULES_PER_ROW; ++i) {
		uint32_t set = EF_U8(AID_SET);

		if (set == 0xffu)
			rule = EF_U8(AID_ALT_ROWS + EF_U8(AID_ALT_SET) * 0x34u + EF_U8(AID_ROW) * 10u + (int32_t)i);
		else
			rule = EF_U8(AID_RULE_ROWS + set * 0x33u + EF_U8(AID_ROW) * 10u + (int32_t)i);
		if (rule == 0xffu)
			break;
		rec = AID_RULES + rule * AID_RULE_SIZE;
		if (EF_U8(rec + RULE_KIND) == 1u && EF_U8(rec + RULE_OBJECT_KIND) == 0u) {
			found = 1u;
			break;
		}
		if (weapon != 0)
			found += schedule_allows_2d78(1u, 2u, (int16_t)rule, (uint32_t)weapon);
		if (target != 0)
			found += schedule_allows_2d78(1u, 3u, (int16_t)rule, (uint32_t)target);
		if (ground != 0)
			found += schedule_allows_2d78(1u, 4u, (int16_t)rule, (uint32_t)ground);
		if (wingman != 0)
			found += schedule_allows_2d78(1u, 5u, (int16_t)rule, (uint32_t)wingman);
		if (threat != 0)
			found += schedule_allows_2d78(1u, 6u, (int16_t)rule, (uint32_t)threat);
		if (threat_type != 0)
			found += schedule_allows_2d78(1u, 7u, (int16_t)rule, (uint32_t)threat_type);
		if (weapon_target != 0)
			found += schedule_allows_2d78(2u, 3u, (int16_t)rule, (uint32_t)weapon_target);
		if (weapon_ground != 0)
			found += schedule_allows_2d78(2u, 4u, (int16_t)rule, (uint32_t)weapon_ground);
		if (found != 0u)
			break;
	}

	if (found == 0u) {
		EF_U16(AID_PENDING) = 0u;
		EF_U8(AID_KIND) = 1u;
		EF_U32(AID_SUBJECT) = obj(AID_PLAYER);
		EF_U8(AID_OBJECT_KIND) = 0u;
		EF_U32(AID_OBJECT) = 0u;
		EF_U16(AID_PARAM) = 100u;
		EF_U8(AID_RULE) = 0u;
		(void)guest_fn_2fbc4_r(STR_AID_FIXED);
		if (EF_U8(PARACHUTE_AID) != 0u)
			(void)guest_fn_2fbc4_r(STR_AID_PARA);
		return;
	}

	rec = AID_RULES + rule * AID_RULE_SIZE;
	switch (EF_U8(rec + RULE_KIND)) {
	case 1:
		EF_U32(AID_SUBJECT) = obj(AID_PLAYER);
		(void)copy_string_9d749(buf, object_name(obj(AID_PLAYER)));
		use_callsign(buf, AID_PLAYER);
		(void)copy_string_9d749(AID_SUBJECT_TEXT, buf);
		break;
	case 2:
		EF_U32(AID_SUBJECT) = obj(AID_WEAPON);
		if (EF_U8(PARACHUTE_AID) != 0u)
			(void)copy_string_9d749(AID_SUBJECT_TEXT, STR_BAILED_PILOT);
		else
			guest_sprintf_a4342(AID_SUBJECT_TEXT, STR_S_PAREN, STR_WEAPON, type_name(obj(AID_WEAPON)));
		break;
	case 3:
		EF_U32(AID_SUBJECT) = obj(AID_TARGET);
		named(buf, AID_TARGET);
		guest_sprintf_a4342(AID_SUBJECT_TEXT, STR_S_PAREN, STR_TARGET, buf);
		break;
	case 4:
		EF_U32(AID_SUBJECT) = obj(AID_GROUND_TARGET);
		(void)copy_string_9d749(AID_SUBJECT_TEXT, STR_GROUND_TARGET);
		break;
	case 5:
		EF_U32(AID_SUBJECT) = obj(AID_WINGMAN);
		named(buf, AID_WINGMAN);
		guest_sprintf_a4342(AID_SUBJECT_TEXT, STR_S_PAREN, STR_WINGMAN, buf);
		break;
	case 6:
		EF_U32(AID_SUBJECT) = obj(AID_THREAT);
		guest_sprintf_a4342(AID_SUBJECT_TEXT, STR_S_PAREN, STR_THREAT, object_name(obj(AID_THREAT)));
		break;
	case 7:
		EF_U32(AID_SUBJECT) = obj(AID_THREAT_TYPE);
		guest_sprintf_a4342(AID_SUBJECT_TEXT, STR_S_PAREN, STR_THREAT, type_name(obj(AID_THREAT_TYPE)));
		break;
	}

	switch (EF_U8(rec + RULE_OBJECT_KIND)) {
	case 0:
		EF_U32(AID_OBJECT) = 0u;
		EF_U8(AID_OBJECT_TEXT) = EF_U8(STR_EMPTY);
		break;
	case 1:
		EF_U32(AID_OBJECT) = obj(AID_PLAYER);
		named(buf, AID_PLAYER);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S, buf);
		break;
	case 2:
		EF_U32(AID_OBJECT) = obj(AID_WEAPON);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S_PAREN, STR_WEAPON, type_name(obj(AID_WEAPON)));
		break;
	case 3:
		EF_U32(AID_OBJECT) = obj(AID_TARGET);
		named(buf, AID_TARGET);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S_PAREN, STR_TARGET2, buf);
		break;
	case 4:
		EF_U32(AID_OBJECT) = obj(AID_GROUND_TARGET);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S, STR_GROUND_TARGET);
		break;
	case 5:
		EF_U32(AID_OBJECT) = obj(AID_WINGMAN);
		named(buf, AID_WINGMAN);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S_PAREN, STR_WINGMAN, buf);
		break;
	case 6:
		EF_U32(AID_OBJECT) = obj(AID_THREAT);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S_PAREN, STR_THREAT, object_name(obj(AID_THREAT)));
		break;
	case 7:
		EF_U32(AID_OBJECT) = obj(AID_THREAT_TYPE);
		guest_sprintf_a4342(AID_OBJECT_TEXT, STR_DASH_S_PAREN, STR_THREAT, type_name(obj(AID_THREAT_TYPE)));
		break;
	}

	EF_U8(AID_KIND) = EF_U8(rec + RULE_KIND);
	EF_U8(AID_OBJECT_KIND) = EF_U8(rec + RULE_OBJECT_KIND);
	EF_U16(AID_PARAM) = EF_U16(rec + RULE_PARAM);
	EF_U8(AID_RULE) = (uint8_t)rule;
	EF_U16(AID_PENDING) = 0u;
	if (guest_strcmp(rec, STR_EMPTY) != 0) {
		(void)guest_fn_2fbc4_r(rec);
		if (EF_U8(PARACHUTE_AID) != 0u)
			(void)guest_fn_2fbc4_r(STR_AID_PARA);
	}
}
