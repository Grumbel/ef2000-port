/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x36-byte records of g_active_record_pool. */
#define UNIT_RECORD_SIZE    0x36u
#define UNIT_STATE          0x0cu  /* int16: 1 = active */
#define UNIT_STRENGTH       0x0eu  /* int16, added to the percentage */
#define UNIT_TYPE           0x10u  /* int16: 0, 1, 2 or 3 */
#define UNIT_HOME_SIDE      0x16u  /* first identity: side, group, member */
#define UNIT_HOME_GROUP     0x18u
#define UNIT_HOME_MEMBER    0x1au
#define UNIT_MODE           0x1cu  /* int16, 1 excludes type 0 from the idle count */
#define UNIT_BUSY           0x1eu  /* int16, nonzero while committed */
#define UNIT_MAP_X          0x20u  /* int16 */
#define UNIT_MAP_Y          0x22u  /* int16, 400 at the top */
#define UNIT_AT_SIDE        0x30u  /* second identity */
#define UNIT_AT_GROUP       0x32u
#define UNIT_AT_MEMBER      0x34u

/* Parser buffer entries (0x2c bytes at +0x0a, count word +0x04). */
#define PARSED_ENTRY_SIZE   0x2cu
#define PARSED_KIND_UNIT    2u
#define PARSED_STRENGTH     0x2bu

#define CONNECTION_MODE_MAP 3u
#define MAP_GRID            0x00134d8au /* 40 x 40 bytes, 10 map units per cell */
#define MAP_GRID_COLUMNS    0x28
#define MAP_GRID_COVERED    1u

static int16_t unit_word(ef_addr32 rec, uint32_t field)
{
    return EF_S16(rec + field);
}

static int unit_on_covered_cell(ef_addr32 rec)
{
    int32_t row = (0x190 - (int32_t)unit_word(rec, UNIT_MAP_Y)) / 10;
    int32_t col = (int32_t)unit_word(rec, UNIT_MAP_X) / 10;

    return EF_U8(MAP_GRID + (uint32_t)(row * MAP_GRID_COLUMNS + col)) == MAP_GRID_COVERED;
}

static void inc_word(ef_addr32 p)
{
    EF_U16(p) = (uint16_t)(EF_U16(p) + 1u);
}

/* 0x11560, 527 bytes. EAX side, EDX group, EBX member, ECX idle count,
 * then the stack pointers home count, percentage, ready count and match
 * flag (all int16); callee cleans 16 bytes. Scan the unit records:
 * active units whose home is (side, group) add to the home count, and
 * one whose home member matches sets the match flag and 0x121ec0.
 * Units at (side, group, member) count as idle (type 0), ready (type 1
 * or 2 and not busy, or type 3 not busy and, in connection mode 3, on a
 * covered map cell) and add the strength of busy type-3 units to the
 * percentage. Parsed type-2 entries add their strength too; with any
 * present the sum is averaged over them. The percentage is capped at
 * 100. The home and idle counts are not cleared first. */
void tally_unit_records_11560(int32_t side_in, int32_t group_in, int32_t member_in, ef_addr32 idle,
                              ef_addr32 home, ef_addr32 percent, ef_addr32 ready, ef_addr32 match)
{
    int16_t side = (int16_t)side_in;
    int16_t group = (int16_t)group_in;
    int16_t member = (int16_t)member_in;
    int16_t parsed = 0;
    int16_t i;

    g_unit_tally_member_seen = 0;
    EF_U16(percent) = 0;
    EF_U16(ready) = 0;
    EF_U16(match) = 0;
    for (i = 0; (int32_t)i < (int32_t)g_record_pool_active_count; ++i) {
        ef_addr32 rec = g_active_record_pool + (uint32_t)((int32_t)i * (int32_t)UNIT_RECORD_SIZE);
        int16_t type;
        int active;

        if (unit_word(rec, UNIT_HOME_SIDE) == side && unit_word(rec, UNIT_HOME_GROUP) == group &&
            unit_word(rec, UNIT_STATE) == 1) {
            inc_word(home);
            if (unit_word(rec, UNIT_HOME_MEMBER) == member) {
                g_unit_tally_member_seen = 1;
                EF_U16(match) = 1;
            }
        }
        if (unit_word(rec, UNIT_AT_SIDE) != side || unit_word(rec, UNIT_AT_GROUP) != group ||
            unit_word(rec, UNIT_AT_MEMBER) != member)
            continue;

        type = unit_word(rec, UNIT_TYPE);
        if (type == 3 && unit_word(rec, UNIT_BUSY) != 0)
            EF_U16(percent) = (uint16_t)(EF_U16(percent) + EF_U16(rec + UNIT_STRENGTH));
        active = unit_word(rec, UNIT_STATE) == 1;
        if (type == 0 && active && unit_word(rec, UNIT_MODE) != 1)
            inc_word(idle);
        if ((type == 2 || type == 1) && active && unit_word(rec, UNIT_BUSY) == 0)
            inc_word(ready);
        if (type == 3 && unit_word(rec, UNIT_BUSY) == 0 && active &&
            (g_frontend_connection_mode != CONNECTION_MODE_MAP || unit_on_covered_cell(rec)))
            inc_word(ready);
    }

    if (g_parser_active_buffer != 0) {
        int16_t k;

        for (k = 0; k < EF_S16(g_parser_active_buffer + 4u); ++k) {
            ef_addr32 e = EF_U32(g_parser_active_buffer + 0xau) + (uint32_t)((int32_t)k * (int32_t)PARSED_ENTRY_SIZE);

            if (EF_U8(e) == PARSED_KIND_UNIT) {
                ++parsed;
                EF_U16(percent) = (uint16_t)(EF_U16(percent) + EF_U8(e + PARSED_STRENGTH));
            }
        }
        if (parsed != 0)
            EF_U16(percent) = (uint16_t)((int32_t)EF_S16(percent) / (int32_t)parsed);
    }
    if (EF_S16(percent) > 100)
        EF_U16(percent) = 100;
}
