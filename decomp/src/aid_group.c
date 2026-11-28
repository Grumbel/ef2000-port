/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Aid.c: choose the unit group the in-flight aid talks about. Groups are
 * the 0xca-byte records at g_world_grid_base; each group's situation code
 * (map_code_3aa8 of its type byte +0x1a) has a priority in the table that
 * init_aid_data_tables_f80 reads from the aid data file. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define AID_PRIORITIES    0x00196fcau /* 27 bytes, priority per situation */
#define AID_FILTER        0x00196fe9u /* which groups the scan considers, see below */
#define AID_REQUEST       0x00196feeu /* word, cleared on every selection */
#define AID_CHANGED       0x00196ff0u
#define AID_STEP          0x00196ff2u /* 0 scan, 2 next group, other values previous group */
#define AID_PLAYER        0x00197061u /* object picked from the group by 0x33b4 */
#define AID_PRIORITY      0x0019707fu /* priority of the current situation */
#define AID_SITUATION     0x00197080u
#define AID_GROUP         0x00197081u /* word */
#define AID_NEXT_CHECK    0x0019708bu /* dword, time until the next selection */
#define WATCHED_GROUP     0x001f71ccu /* word, group of the watched unit (g_watched_unit) */

/* Group record fields. */
#define GROUP_KIND        0x15u /* int16 */
#define GROUP_TYPE        0x1au
#define GROUP_SIDE        0x1cu /* int16 */
#define GROUP_ACTIVE      0x20u
#define GROUP_MEMBERS     0x4fu
#define GROUP_LEADER      0x50u /* int16 object id of the first member */
#define GROUP_REFERENCE   0xb5u /* pointer to a grid reference block, or 0 */

#define SITUATION_NONE    0xffu
#define NEAR_TARGET       3200000u
#define NEAR_REFERENCE    0x271000u

static ef_addr32 group_record(int16_t group)
{
    return g_world_grid_base + (uint32_t)((int32_t)group * 0xca);
}

static ef_addr32 group_leader(int16_t group)
{
    return lookup_filtered_indexed_object((int16_t)EF_U16(group_record(group) + GROUP_LEADER));
}

/* Largest absolute axis delta between two objects, z first like the original. */
static uint32_t object_distance(ef_addr32 a, ef_addr32 b)
{
    uint32_t dz = abs_a28f2((int32_t)(EF_U32(a + 8u) - EF_U32(b + 8u)));
    uint32_t dy = abs_a28f2((int32_t)(EF_U32(a + 4u) - EF_U32(b + 4u)));
    uint32_t dx = abs_a28f2((int32_t)(EF_U32(a) - EF_U32(b)));

    return (uint32_t)max3_signed((int32_t)dx, (int32_t)dy, (int32_t)dz);
}

/* Distance from OBJ to the centre of the grid cell in reference block REF
 * (cell words at +0/+2, offsets at +0x18/+0x1a, height at +0x1c), the same
 * point 0x33b4 uses. */
static uint32_t reference_distance(ef_addr32 obj, ef_addr32 ref)
{
    uint32_t x = ((uint32_t)(EF_S32(ref) >> 16) << 19) +
                 (((uint32_t)(EF_S32(ref + 0x18u) >> 16) << 8) + 0x40000u);
    uint32_t y = (uint32_t)(-(EF_S32(ref + 0x1au) >> 16)) << 8;
    uint32_t z = ((uint32_t)(int32_t)(int16_t)EF_U16(ref) << 19) +
                 (((uint32_t)(EF_S32(ref + 0x16u) >> 16) << 8) + 0x40000u);
    uint32_t dz = abs_a28f2((int32_t)(EF_U32(obj + 8u) - z));
    uint32_t dy = abs_a28f2((int32_t)(EF_U32(obj + 4u) - y));
    uint32_t dx = abs_a28f2((int32_t)(EF_U32(obj) - x));

    return (uint32_t)max3_signed((int32_t)dx, (int32_t)dy, (int32_t)dz);
}

/* 0x2e04, 1456 bytes. Select the group the aid follows. A pending step
 * request (AID_STEP) moves to the next or previous active group with
 * members, wrapping around, and takes its situation as is. Otherwise every
 * active group with a type and members that passes AID_FILTER (0 only the
 * watched group, 1 groups on the watched group's side, 2 kind 1, 3 kind 0,
 * 4 kind 2, other values all groups) is rated: situation 1 (leader with a
 * target) becomes 13 when the target is 3,200,000 or more away, situation 3
 * falls back to 0 when the leader is beyond 0x271000 of its reference
 * point, and other situations with a reference point within 0x271000
 * become 3. The highest priority wins, and among equal priorities with a
 * distance the nearest wins (situations without one tie in favour of the
 * later group). When 0x33b4 then picks a different object, the group and
 * its situation become current. The next selection is due in 500..950
 * ticks after situation 3, else in 1500..2950. */
void select_aid_group_2e04(void)
{
    uint8_t best = SITUATION_NONE;
    uint32_t best_priority = EF_U8(AID_PRIORITY);
    uint32_t best_distance = 0xffffffffu;
    int16_t best_group = 0; /* the original leaves it unset when nothing is chosen; then it is unused */
    int16_t watched_side;
    uint8_t step;

    EF_U16(AID_REQUEST) = 0;
    watched_side = (int16_t)EF_U16(group_record((int16_t)EF_U16(WATCHED_GROUP)) + GROUP_SIDE);
    step = EF_U8(AID_STEP);
    if (step != 0u) {
        int16_t delta = step == 2u ? 1 : -1;
        uint16_t start = EF_U16(AID_GROUP);
        uint16_t last = (uint16_t)(g_unit_count - 1);
        uint16_t group = start;

        EF_U8(AID_STEP) = 0;
        for (;;) {
            ef_addr32 rec;

            group = (uint16_t)(group + delta);
            if ((int16_t)group < 0)
                group = last;
            if (group == (uint16_t)g_unit_count)
                group = 0;
            if (group == start)
                break;
            rec = group_record((int16_t)group);
            if (EF_U8(rec + GROUP_ACTIVE) != 0u && EF_U8(rec + GROUP_MEMBERS) != 0u)
                break;
        }
        best_group = (int16_t)group;
        best = map_code_3aa8(EF_U8(group_record(best_group) + GROUP_TYPE));
    } else {
        int16_t i;

        for (i = 0; i < g_unit_count; i++) {
            ef_addr32 rec = group_record(i);
            ef_addr32 obj;
            uint8_t situation;
            uint32_t distance = 0;
            int has_distance = 0;

            if (EF_U8(rec + GROUP_ACTIVE) == 0u || EF_U8(rec + GROUP_TYPE) == 0u ||
                EF_U8(rec + GROUP_MEMBERS) == 0u)
                continue;
            switch (EF_U8(AID_FILTER)) {
            case 0:
                if (i != (int16_t)EF_U16(WATCHED_GROUP))
                    continue;
                break;
            case 1:
                if ((int16_t)EF_U16(rec + GROUP_SIDE) != watched_side)
                    continue;
                break;
            case 2:
                if ((int16_t)EF_U16(rec + GROUP_KIND) != 1)
                    continue;
                break;
            case 3:
                if (EF_U16(rec + GROUP_KIND) != 0u)
                    continue;
                break;
            case 4:
                if ((int16_t)EF_U16(rec + GROUP_KIND) != 2)
                    continue;
                break;
            }
            situation = map_code_3aa8(EF_U8(rec + GROUP_TYPE));
            if (EF_U8(AID_PRIORITIES + situation) < best_priority)
                continue;
            if (situation == 1u) {
                obj = group_leader(i);
                if (obj == 0u)
                    continue;
                if (EF_U16(obj + 0x146u) == 0xffu) {
                    distance = 0x269300u;
                } else {
                    ef_addr32 target = lookup_filtered_indexed_object((int16_t)EF_U16(obj + 0x146u));

                    if (target == 0u)
                        continue;
                    distance = object_distance(obj, target);
                    if (distance < NEAR_TARGET)
                        distance = 0x261600u;
                    else
                        situation = 13;
                }
                has_distance = 1;
            } else if (situation == 4u) {
                obj = group_leader(i);
                if (obj == 0u || (int16_t)EF_U16(obj + 0x16au) == 0xff)
                    continue;
                distance = object_distance(obj, lookup_scene_node((int16_t)EF_U16(obj + 0x16au)));
                has_distance = 1;
            } else if (situation == 3u) {
                if (EF_U32(group_record(i) + GROUP_REFERENCE) != 0u) {
                    obj = group_leader(i);
                    if (obj == 0u)
                        continue;
                    distance = reference_distance(obj, EF_U32(group_record(i) + GROUP_REFERENCE));
                    if (distance <= NEAR_REFERENCE)
                        has_distance = 1;
                    else
                        situation = 0;
                }
            } else {
                if (EF_U32(group_record(i) + GROUP_REFERENCE) != 0u) {
                    obj = group_leader(i);
                    if (obj == 0u)
                        continue;
                    distance = reference_distance(obj, EF_U32(group_record(i) + GROUP_REFERENCE));
                    if (distance < NEAR_REFERENCE) {
                        situation = 3;
                        has_distance = 1;
                    }
                }
            }
            if (EF_U8(AID_PRIORITIES + situation) < best_priority)
                continue;
            if (has_distance) {
                if (distance > best_distance)
                    continue;
                best_distance = distance;
            }
            best = situation;
            best_group = i;
            best_priority = EF_U8(AID_PRIORITIES + situation);
        }
    }

    if (best != SITUATION_NONE) {
        ef_addr32 before = EF_U32(AID_PLAYER);

        find_nearest_object_33b4(best_group);
        if (before != EF_U32(AID_PLAYER)) {
            EF_U8(AID_SITUATION) = best;
            EF_U8(AID_PRIORITY) = EF_U8(AID_PRIORITIES + best);
            EF_U8(AID_CHANGED) = 1;
            EF_U16(AID_GROUP) = (uint16_t)best_group;
        }
    }
    if (best == 3u)
        EF_U32(AID_NEXT_CHECK) = (uint32_t)((int32_t)(half_word_9c960() & 0xffffu) % 10 * 50 + 500);
    else
        EF_U32(AID_NEXT_CHECK) = (uint32_t)((int32_t)(half_word_9c960() & 0xffffu) % 30 * 50 + 1500);
}
