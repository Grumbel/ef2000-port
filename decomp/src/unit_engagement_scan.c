/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define UNIT_TABLE          0x0019bb58u /* pointer to 0xca-byte unit records */
#define UNIT_COUNT          0x0019bb5cu /* signed word */
#define UNIT_RECORD_SIZE    0xcau
#define UNIT_SIDE           0x15u       /* signed word: side compared outside mode 3 */
#define UNIT_GROUP          0x17u       /* signed word: row/column in the relation grid */
#define UNIT_POSITION_X     0x61u       /* double */
#define UNIT_POSITION_Y     0x69u       /* double */
#define RELATION_GRID       0x001379c8u /* pointer to signed bytes */
#define RELATION_WIDTH      0x001379ccu /* signed word */
#define CONNECTION_MODE     0x00125846u
#define ENGAGE_RANGE        0x001817d3u /* double */

static int32_t unit_word(ef_addr32 record, uint32_t field)
{
    return (int16_t)EF_U16(record + field);
}

static int out_of_range(double a, double b)
{
    double d = a - b;

    if (d < 0.0)
        d = -d;
    return d > EF_F64(ENGAGE_RANGE);
}

/* 0x16034, 416 bytes. Let unit INDEX try an engagement against every unit
 * within ENGAGE_RANGE on both position axes. Nothing happens unless 0x169b4
 * reports the unit in the right state and 0x174f8 allows its kind. In
 * connection mode 3 a target qualifies when either relation byte between
 * the two groups (grid[mine][theirs] or grid[theirs][mine]) is negative;
 * otherwise when it is on a different side. The target must also pass
 * 0x174f8. 0x168d8 then gets INDEX, the target and SKIP's low byte.
 *
 * The record index J advances by one per pass, but the loop bound is
 * checked against a separate counter that steps over INDEX and 0xff. */
void scan_engagements_16034(uint32_t index, uint32_t skip)
{
    uint32_t offset = index * UNIT_RECORD_SIZE;
    int32_t my_side = unit_word(EF_U32(UNIT_TABLE) + offset, UNIT_SIDE);
    ef_addr32 me, position, them;
    int32_t row, their_group, column;
    int32_t counter;
    uint32_t j;
    int8_t mine_to_theirs, theirs_to_mine;

    if ((uint16_t)unit_in_state_169b4((int32_t)index) == 0u)
        return;
    if ((uint8_t)unit_kind_allows_174f8(index) == 0u)
        return;
    me = EF_U32(UNIT_TABLE) + offset;
    row = unit_word(me, UNIT_GROUP) * (int16_t)EF_U16(RELATION_WIDTH);
    position = me + UNIT_POSITION_X;
    j = 0;
    for (counter = 0; counter < (int16_t)EF_U16(UNIT_COUNT);
         counter += 1 + (counter == (int32_t)index) + (counter == 0xff), j++) {
        them = EF_U32(UNIT_TABLE) + j * UNIT_RECORD_SIZE;
        their_group = unit_word(them, UNIT_GROUP);
        column = their_group * (int16_t)EF_U16(RELATION_WIDTH);
        if (out_of_range(EF_F64(them + UNIT_POSITION_X), EF_F64(position)))
            continue;
        if (out_of_range(EF_F64(them + UNIT_POSITION_Y), EF_F64(position + 8u)))
            continue;
        if (EF_U8(CONNECTION_MODE) == 3u) {
            me = EF_U32(UNIT_TABLE) + offset;
            mine_to_theirs = (int8_t)EF_U8(EF_U32(RELATION_GRID) + row + their_group);
            theirs_to_mine = (int8_t)EF_U8(EF_U32(RELATION_GRID) + column + unit_word(me, UNIT_GROUP));
            if ((uint8_t)unit_kind_allows_174f8(j) == 0u)
                continue;
            if (mine_to_theirs >= 0 && theirs_to_mine >= 0)
                continue;
        } else {
            if ((uint8_t)unit_kind_allows_174f8(j) == 0u)
                continue;
            if (unit_word(EF_U32(UNIT_TABLE) + j * UNIT_RECORD_SIZE, UNIT_SIDE) == my_side)
                continue;
        }
        try_engagement_168d8(index, j, skip & 0xffu);
    }
}
