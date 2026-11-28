/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Write the combat result table ("Übersicht Zusammenstöße") to a text
 * stream. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define RESULT_TABLE      0x001379bcu /* pointer to 54-byte result records */
#define RESULT_COUNT      0x001379b0u /* int32 */
#define RESULT_SIZE       0x36u
#define GROUP_LIST        0x00137960u /* group nodes, record pointer +0x13 (type byte +0) */
#define UNIT_LIST         0x00137970u /* unit nodes keyed by word +0x13, type byte +0xc */
#define STATIC_LIST       0x00137988u /* static object nodes, type byte +0xb */

/* Result record fields; words unless noted. */
#define RESULT_WEAPON           0x09u /* byte */
#define RESULT_SHOOTER_CATEGORY 0x0au /* 0 plane, 1 ship, 2 ground */
#define RESULT_OUTCOME          0x0cu /* 5: no result, not written */
#define RESULT_TARGET_CATEGORY  0x10u /* 0 plane, 1 ship, 2 ground, 3 static */
#define RESULT_TARGET_STATIC    0x12u
#define RESULT_TARGET_MISSION   0x16u
#define RESULT_TARGET_GROUP     0x18u
#define RESULT_TARGET           0x1au
#define RESULT_TIME             0x2au /* day, hour, minute */
#define RESULT_SHOOTER_MISSION  0x30u
#define RESULT_SHOOTER_GROUP    0x32u
#define RESULT_SHOOTER          0x34u

#define FMT_HEADER        0x00131df8u /* "Übersicht Zusammenstöße\n\n\n" */
#define FMT_MISSIONS      0x00131e13u /* "ZIELEINSÄTZE #%d : ABSCHUSSEINSÄTZE #%d" */
#define FMT_GROUPS        0x00131e3bu /* "ZIELGRUPPE #%d : ABSCHUSSGRUPPE #%d" */
#define FMT_UNITS         0x00132850u /* "ZIEL #%d : ANGREIFER #%d\n" */
#define FMT_CATEGORY      0x00131e60u /* "ZIELKATEGORIE %s \n" */
#define FMT_NAMES         0x00131e85u /* "Ziel: %s  Abschuß: %s\n" */
#define FMT_TIME          0x00131e9cu /* "KOLLISIONSZEIT: Tag %d   %d Uhr ..." */
#define FMT_WEAPON        0x00131ec9u /* "BENUTZTE WAFFE: %s\n" */
#define FMT_OUTCOME       0x00131eddu /* "ERGEBNIS %s\n\n" */
#define STR_NA            0x0018b130u /* "NA" */
#define STR_FLAK          0x00131f26u /* "Flak-Geschütze" */
#define STR_SAM           0x00131f35u /* "SAM-Raketenwerfer" */

#define NAMES_PLANE       0x00132dd0u /* string pointers indexed by unit type */
#define NAMES_SHIP        0x00133440u
#define NAMES_GROUND      0x001334f4u
#define NAMES_STATIC      0x0013315cu
#define NAMES_CATEGORY    0x001336c8u
#define NAMES_WEAPON      0x00133634u
#define NAMES_OUTCOME     0x00132d00u

static int32_t result_word(ef_addr32 result, uint32_t offset)
{
    return (int16_t)EF_U16(result + offset);
}

static ef_addr32 result_at(int16_t i)
{
    return EF_U32(RESULT_TABLE) + (uint32_t)((int32_t)i * (int32_t)RESULT_SIZE);
}

static ef_addr32 table_entry(ef_addr32 table, int32_t index)
{
    return EF_U32(table + (uint32_t)(index * 4));
}

static int16_t group_type(int32_t group)
{
    return (int16_t)EF_U8(EF_U32(list_nth(GROUP_LIST, (uint32_t)group) + 0x13u));
}

/* Type of a unit: the type byte of its node in UNIT_LIST, or that of its
 * mission's group when no node has the unit's number. */
static int16_t unit_type(int32_t unit, int32_t mission)
{
    ef_addr32 node = find_node_by_word_c1b8c((uint16_t)unit, UNIT_LIST);

    if (node != 0u)
        return (int16_t)EF_U8(node + 0xcu);
    return group_type(mission);
}

static void copy_na(uint8_t *buffer)
{
    buffer[0] = EF_U8(STR_NA);
    buffer[1] = EF_U8(STR_NA + 1u);
    buffer[2] = EF_U8(STR_NA + 2u);
}

/* Name of a unit type in CATEGORY 0..2; returns 0 for other categories. */
static ef_addr32 unit_name(int32_t category, int16_t type)
{
    if (category == 0)
        return table_entry(NAMES_PLANE, type);
    if (category == 1)
        return table_entry(NAMES_SHIP, type);
    if (category == 2)
        return table_entry(NAMES_GROUND, type);
    return 0;
}

/* 0x9b2a4, 961 bytes. Write a header and, for every result other than 5
 * ("Kein Ergebnis"), the target and shooter missions, groups and units, the
 * target category, both names, the time, weapon and outcome. The target
 * name comes from its category's table (statics by the static object's
 * type) or is "NA" when the type is unknown; the shooter name, without a
 * shooter, is flak, SAM or "NA" by weapon. A name whose category selects
 * no table keeps the previous result's text, as with the original's stack
 * buffers. */
void write_combat_log_9b2a4(ef_addr32 stream)
{
    uint8_t target_name[0x34];
    uint8_t shooter_name[0x34];
    int16_t i;

    guest_fn_bc400(stream, FMT_HEADER);
    for (i = 0; (int32_t)i < (int32_t)EF_U32(RESULT_COUNT); i++) {
        ef_addr32 result = result_at(i);
        int16_t target_type, shooter_type;
        int32_t category;
        ef_addr32 name;

        if (result_word(result, RESULT_OUTCOME) == 5)
            continue;
        guest_fn_bc400(stream, FMT_MISSIONS, result_word(result, RESULT_TARGET_MISSION),
                       result_word(result, RESULT_SHOOTER_MISSION));
        result = result_at(i);
        guest_fn_bc400(stream, FMT_GROUPS,
                       result_word(result, RESULT_TARGET_MISSION) == -1 ? -1
                                                                        : result_word(result, RESULT_TARGET_GROUP),
                       result_word(result, RESULT_SHOOTER_GROUP));
        result = result_at(i);
        guest_fn_bc400(stream, FMT_UNITS, result_word(result, RESULT_TARGET), result_word(result, RESULT_SHOOTER));
        result = result_at(i);
        guest_fn_bc400(stream, FMT_CATEGORY, table_entry(NAMES_CATEGORY, result_word(result, RESULT_TARGET_CATEGORY)));

        result = result_at(i);
        if (result_word(result, RESULT_TARGET) != -1)
            target_type = unit_type(result_word(result, RESULT_TARGET), result_word(result_at(i), RESULT_TARGET_MISSION));
        else if (result_word(result, RESULT_TARGET_MISSION) != -1)
            target_type = group_type(result_word(result, RESULT_TARGET_MISSION));
        else
            target_type = -1;
        result = result_at(i);
        if (result_word(result, RESULT_SHOOTER) != -1)
            shooter_type = unit_type(result_word(result, RESULT_SHOOTER),
                                     result_word(result_at(i), RESULT_SHOOTER_MISSION));
        else
            shooter_type = -1;

        result = result_at(i);
        category = result_word(result, RESULT_TARGET_CATEGORY);
        if (category >= 0 && category <= 2)
            copy_string_9d749((ef_addr32)(uintptr_t)target_name, unit_name(category, target_type));
        else if (category == 3)
            copy_string_9d749((ef_addr32)(uintptr_t)target_name,
                              table_entry(NAMES_STATIC,
                                          EF_U8(list_nth_word((uint16_t)result_word(result, RESULT_TARGET_STATIC),
                                                              STATIC_LIST) + 0xbu)));
        else if (target_type == -1)
            copy_na(target_name);

        if (shooter_type != -1) {
            category = result_word(result_at(i), RESULT_SHOOTER_CATEGORY);
            name = unit_name(category, shooter_type);
            if (category >= 0 && category <= 2)
                copy_string_9d749((ef_addr32)(uintptr_t)shooter_name, name);
        } else {
            uint32_t weapon;

            result = result_at(i);
            weapon = EF_U8(result + RESULT_WEAPON);
            if (weapon == 0x1bu && EF_U16(result + RESULT_TARGET_CATEGORY) == 0u)
                copy_string_9d749((ef_addr32)(uintptr_t)shooter_name, STR_FLAK);
            else if (weapon == 0x21u || weapon == 0x22u || weapon == 0x23u || weapon == 0x24u)
                copy_string_9d749((ef_addr32)(uintptr_t)shooter_name, STR_SAM);
            else
                copy_na(shooter_name);
        }

        guest_fn_bc400(stream, FMT_NAMES, (ef_addr32)(uintptr_t)target_name, (ef_addr32)(uintptr_t)shooter_name);
        result = result_at(i);
        guest_fn_bc400(stream, FMT_TIME, result_word(result, RESULT_TIME), result_word(result, RESULT_TIME + 2u),
                       result_word(result, RESULT_TIME + 4u));
        result = result_at(i);
        guest_fn_bc400(stream, FMT_WEAPON, table_entry(NAMES_WEAPON, EF_U8(result + RESULT_WEAPON)));
        result = result_at(i);
        guest_fn_bc400(stream, FMT_OUTCOME, table_entry(NAMES_OUTCOME, result_word(result, RESULT_OUTCOME)));
    }
}
