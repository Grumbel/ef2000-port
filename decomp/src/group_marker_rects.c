/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define NODE_NEXT    0x04u
#define NODE_VALUE   0x09u
#define NODE_TYPE    0x0au
#define NODE_RECORD  0x13u /* record: kind byte +0, position pointer +0x20 */
#define RECORD_POSITION 0x20u

#define TYPE_PAIRED  0x14u /* counts when 0xcc81c accepts value and kind */
#define TYPE_MARK_A  0x1cu
#define TYPE_MARK_B  0x1du

#define MARKER_HALF  12
#define MARKER_LIMIT 0x18f

static int node_is_marked(ef_addr32 node)
{
    uint32_t type = EF_U8(node + NODE_TYPE);

    if (type == TYPE_PAIRED
        && (uint16_t)pair_matches_cc81c(EF_U8(node + NODE_VALUE), EF_U8(EF_U32(node + NODE_RECORD))) != 0u)
        return 1;
    type = EF_U8(node + NODE_TYPE);
    return type == TYPE_MARK_A || type == TYPE_MARK_B;
}

static uint32_t clamp_coordinate(int32_t value)
{
    if (value > MARKER_LIMIT)
        value = MARKER_LIMIT;
    if (value < 0)
        value = 0;
    return (uint32_t)value << 11;
}

/* 0xf918, 504 bytes. EAX list head (0x137960 from both callers). Count
 * the marked nodes (paired type 0x14 accepted by 0xcc81c, or types 0x1c
 * and 0x1d), allocate 16 bytes per node at 0x1a5904 (0 when none) and
 * store each node's 24-unit box around its position, clamped to
 * 0..0x18f and scaled by 0x800: left, bottom, right, top. The pointer
 * global's existing name predates this use. Called at 0xb273 and 0x505cf. */
void build_marker_rects_f918(ef_addr32 head)
{
    ef_addr32 node;
    int16_t count = 0;
    int16_t index = 0;

    for (node = EF_U32(head); node != 0u; node = EF_U32(node + NODE_NEXT))
        if (node_is_marked(node))
            ++count;
    if (count == 0) {
        g_secondary_parser_text = 0u;
        return;
    }
    g_secondary_parser_text = log_alloc_e764((uint32_t)((int32_t)count << 4));
    for (node = EF_U32(head); node != 0u; node = EF_U32(node + NODE_NEXT)) {
        ef_addr32 position;
        ef_addr32 rect;

        if (!node_is_marked(node))
            continue;
        position = EF_U32(EF_U32(node + NODE_RECORD) + RECORD_POSITION);
        rect = g_secondary_parser_text + (uint32_t)((int32_t)index << 4);
        EF_U32(rect) = clamp_coordinate(EF_S32(position) - MARKER_HALF);
        EF_U32(rect + 4u) = clamp_coordinate(EF_S32(position + 4u) + MARKER_HALF);
        EF_U32(rect + 8u) = clamp_coordinate(EF_S32(position) + MARKER_HALF);
        EF_U32(rect + 12u) = clamp_coordinate(EF_S32(position + 4u) - MARKER_HALF);
        ++index;
    }
}
