/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Group node: next at +4, kind bytes at +9/+0xa, index word at +0x10,
 * member count byte at +0x12, member array at +0x13. */
#define GROUP_NEXT(g)         EF_U32((g) + 0x04u)
#define GROUP_STATE(g)        EF_U8((g) + 0x09u)
#define GROUP_TYPE(g)         EF_U8((g) + 0x0au)
#define GROUP_INDEX(g)        EF_U16((g) + 0x10u)
#define GROUP_COUNT(g)        EF_U8((g) + 0x12u)
#define GROUP_MEMBERS(g)      EF_U32((g) + 0x13u)

#define MEMBER_SIZE           0x7cu
#define MEMBER_STATE(m)       EF_U8((m) + 0x03u)
#define MEMBER_DATA           0x08u   /* 18 bytes from the update record */
#define MEMBER_DATA_SIZE      18u
#define MEMBER_TAG(m)         EF_U16((m) + 0x1eu)

/* Member update record, 27 bytes. */
#define UPDATE_SIZE           0x1bu
#define UPDATE_KIND(u)        EF_U8((u) + 0x00u)
#define UPDATE_TAG(u)         EF_U16((u) + 0x01u)
#define UPDATE_GROUP(u)       EF_U16((u) + 0x03u)
#define UPDATE_MEMBER(u)      EF_S16((u) + 0x05u)
#define UPDATE_DATA           0x09u

enum { UPDATE_REMOVE = 0, UPDATE_SET = 1, UPDATE_DESTROY = 2 };
enum { MEMBER_LIVE = 0, MEMBER_REMOVED = 1, MEMBER_DESTROYED = 9 };

#define GROUP_STATE_PLAIN     0
#define GROUP_STATE_DONE      2
#define STR_PURGE_ALLOC       0x0018f328u  /* "Unable to alloc [%d] for groups in PurgeG..." */

static void remove_group(ef_addr32 group, ef_addr32 head, ef_addr32 tail)
{
    unlink_node_bc594(group, head, tail);
    free_object_array_b17d4(group);
}

static int is_finished_group(ef_addr32 group, ef_addr32 list, uint32_t extra)
{
    uint8_t type = GROUP_TYPE(group);

    if (GROUP_STATE(group) == GROUP_STATE_DONE)
        return 1;
    if (type == 0x14 || type == 0x1b || type == 0x1c || type == 0x1d)
        return 1;
    return GROUP_STATE(group) == GROUP_STATE_PLAIN
        && (uint16_t)list_has_matching_entry_cb984(group, list, extra) != 0;
}

/* Apply the update records addressed to one member of a group. */
static void apply_updates(ef_addr32 group, int32_t index, ef_addr32 member,
                          ef_addr32 updates, int32_t update_count)
{
    ef_addr32 u = updates;
    int32_t k;
    uint32_t b;

    for (k = 0; k < update_count; k++, u += UPDATE_SIZE) {
        if (UPDATE_GROUP(u) != GROUP_INDEX(group) || UPDATE_MEMBER(u) != index)
            continue;
        switch (UPDATE_KIND(u)) {
        case UPDATE_REMOVE:
            MEMBER_STATE(member) = MEMBER_REMOVED;
            break;
        case UPDATE_SET:
            MEMBER_STATE(member) = MEMBER_LIVE;
            for (b = 0; b < MEMBER_DATA_SIZE; b++)
                EF_U8(member + MEMBER_DATA + b) = EF_U8(u + UPDATE_DATA + b);
            MEMBER_TAG(member) = UPDATE_TAG(u);
            break;
        case UPDATE_DESTROY:
            MEMBER_STATE(member) = MEMBER_DESTROYED;
            break;
        }
    }
}

/* Drop the non-live members of a group, or the whole group when none is live. */
static void compact_group(ef_addr32 group, ef_addr32 head, ef_addr32 tail)
{
    uint32_t count = GROUP_COUNT(group);
    uint32_t live = 0, j, b, size, dst;
    ef_addr32 members, member, packed;

    for (j = 0; j < count; j++)
        if (MEMBER_STATE(GROUP_MEMBERS(group) + j * MEMBER_SIZE) == MEMBER_LIVE)
            live++;
    if (live == 0) {
        remove_group(group, head, tail);
        return;
    }
    if (live >= count)
        return;

    size = live * MEMBER_SIZE;
    packed = guest_fn_cc6c0_z(size, STR_PURGE_ALLOC, size);
    fill_bytes_b3fb0(packed, 0, size);
    dst = 0;
    for (j = 0; j < GROUP_COUNT(group); j++) {
        member = GROUP_MEMBERS(group) + j * MEMBER_SIZE;
        if (MEMBER_STATE(member) == MEMBER_LIVE) {
            for (b = 0; b < MEMBER_SIZE; b += 4)
                EF_U32(packed + dst + b) = EF_U32(member + b);
            dst += MEMBER_SIZE;
        } else {
            free_object_b1740(member);
        }
    }
    members = GROUP_MEMBERS(group);
    if (members != 0) {
        tail_jump_cc768(members);
        GROUP_MEMBERS(group) = 0;
    }
    GROUP_MEMBERS(group) = packed;
    GROUP_COUNT(group) = (uint8_t)live;
}

/* 0xcb9f4, 608 bytes (PurgeGroups). Walk the group list at *head: remove
 * finished groups (state 2, types 0x14/0x1b/0x1c/0x1d, or plain groups
 * 0xcb984 matches in list), and apply the member update records to the
 * others (0 marks a member removed, 1 revives it with new data, 2 marks
 * it destroyed). Then compact each group to its live members, removing
 * groups without one, and renumber the remaining groups. */
void purge_groups_cb9f4(ef_addr32 head, ef_addr32 tail, ef_addr32 list, uint32_t extra,
                        ef_addr32 updates, int32_t update_count)
{
    ef_addr32 group, next;
    int32_t j;
    uint16_t index;

    for (group = EF_U32(head); group != 0; group = next) {
        next = GROUP_NEXT(group);
        if (is_finished_group(group, list, extra)) {
            remove_group(group, head, tail);
            continue;
        }
        for (j = 0; j < (int32_t)GROUP_COUNT(group); j++)
            apply_updates(group, j, GROUP_MEMBERS(group) + (uint32_t)j * MEMBER_SIZE,
                          updates, update_count);
    }

    for (group = EF_U32(head); group != 0; group = next) {
        next = GROUP_NEXT(group);
        compact_group(group, head, tail);
    }

    index = 0;
    for (group = EF_U32(head); group != 0; group = GROUP_NEXT(group))
        GROUP_INDEX(group) = index++;
}
