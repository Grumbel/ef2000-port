/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record reactivation around a cell for the entry bookkeeping. */

/* 0xc08d4, 270 bytes. For each record at the object's position with the given type, reactivate state-7 records (counting them in the object's tables, capped at 0x31) and state-2 records. */
void reactivate_records_at_object_c08d4(uint8_t type, ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);

    while (rec != 0 && EF_U8(object + 0x32u) < 0x31) {
        if (EF_U8(rec + 0xbu) == 0) {
            if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x11u)
                && EF_U32(rec + 0x3fu) == EF_U32(object + 0x15u)
                && EF_U32(rec + 0x43u) == EF_U32(object + 0x19u)
                && EF_U8(rec + 0xeu) == type
                && EF_U8(rec + 0xdu) == 7) {
                uint32_t n;
                uint32_t v;

                EF_U8(rec + 0xdu) = 0;
                EF_U8(rec + 0x15u) = 100;
                n = (uint32_t)EF_U8(object + 0x32u) + 1u;
                if (n > 0x31)
                    n = 0x31;
                EF_U8(object + 0x32u) = (uint8_t)n;
                v = (uint32_t)EF_U8(EF_U32(object + 0x33u) + EF_U8(rec + 0xcu)) + 1u;
                if (v > 0x31)
                    v = 0x31;
                EF_U8(EF_U32(object + 0x33u) + EF_U8(rec + 0xcu)) = (uint8_t)v;
            }
        } else if (EF_U8(rec + 0xbu) == 2) {
            if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x11u)
                && EF_U32(rec + 0x3fu) == EF_U32(object + 0x15u)
                && EF_U32(rec + 0x43u) == EF_U32(object + 0x19u)
                && EF_U8(rec + 0xeu) == type) {
                EF_U8(rec + 0xdu) = 0;
                EF_U8(rec + 0x15u) = 100;
            }
        }
        rec = EF_U32(rec + 4u);
    }
}

/* 0xc09e4, 202 bytes. Refill an object's sub-entries of kind 0x35/0x37, then reactivate matching non-idle records up to the free capacity of its cell (0x30 minus the cell's record count). */
void refill_object_cell_c09e4(ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t i = 0;
    int16_t limit;
    int16_t done = 0;

    while (i < EF_S16(object + 0x2au)) {
        ef_addr32 sub = EF_U32(object + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;
        uint8_t code = EF_U8(sub);

        if (code == 0x35 || code == 0x37)
            EF_U8(sub + 5u) = 100;
        i++;
    }
    limit = (int16_t)(0x30 - (int16_t)count_records_at_cell_bfb2c((int16_t)EF_S16(object + 0x16u), (int16_t)EF_S16(object + 0x1au), list));
    if (limit < 0)
        limit = 0;
    while (rec != 0 && done < limit) {
        if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x16u)
            && EF_U32(rec + 0x3fu) == EF_U32(object + 0x1au)
            && EF_U8(rec + 0xeu) == EF_U8(object + 9u)
            && EF_U8(rec + 0xdu) != 0) {
            EF_U8(rec + 0xdu) = 0;
            EF_U8(rec + 0x15u) = 100;
            done++;
            EF_U16(rec + 9u) = 0;
        }
        rec = EF_U32(rec + 4u);
    }
}

#define MOVE_TURN   0x00137990u /* word: turn counter */
#define MOVE_SIDE   0x00146e30u /* word: side to move, 0 or 1 */
#define MOVE_NONE   10u
/* The original re-reads the side for every use. */
#define MOVE_SIDE_BYTE EF_U8(MOVE_SIDE)

enum { MOVE_SKIP, MOVE_ONE, MOVE_PAIR };

/* Move RECORD into the box: point its node at SIDE, take it off the cells
 * of LIST, mark it as SIDE's and put it back; returns its node. */
static ef_addr32 move_record_c0b04(ef_addr32 record, ef_addr32 list, ef_addr32 nodes)
{
    ef_addr32 node = find_node_by_record_a(record, nodes);

    EF_U8(node + 9u) = MOVE_SIDE_BYTE;
    deactivate_records_at_object_c07a0(record, list);
    EF_U8(record + 0x0bu) = MOVE_SIDE_BYTE;
    reactivate_records_at_object_c08d4(MOVE_SIDE_BYTE, record, list);
    return node;
}

/* 0xc0b04, 1326 bytes, three stack arguments (callee pops; the third is
 * unused): one move of the side whose turn it is. The side alternates with
 * the turn counter, unless the performance display (0x1a364c) forces side 0
 * above 70 or side 1 below 30 without counting the turn. If the side has a
 * movable record (0xcc468, 0xcc51c), its record and group are looked up;
 * the record's type (+9) decides whether it moves alone, together with its
 * sibling (0xb4c7c) or with its child (0xb4c20), some types also marking
 * list entries. A move needs every moved record inside the box (0xbfb88);
 * then the moved records' and the group's cells are refilled and the side
 * returned. Otherwise 10. The side is 0 or 1, so the side switch over
 * 0..5 reduces to these two cases.
 *
 * Types not listed below leave the original's action word uninitialised
 * (stack residue); here they make no move. */
uint16_t move_side_record_c0b04(ef_addr32 head_a, ef_addr32 head_b, ef_addr32 list, ef_addr32 nodes,
                                uint32_t context, ef_addr32 box, uint32_t unused)
{
    uint16_t turn = (uint16_t)(EF_U16(MOVE_TURN) + 1u);
    ef_addr32 record;
    ef_addr32 group;
    ef_addr32 partner = 0;
    ef_addr32 node;
    int action = MOVE_SKIP;

    (void)unused;
    if (g_performance_display != 0u && g_performance_value > 0x46u) {
        EF_U16(MOVE_SIDE) = 0;
    } else if (g_performance_display != 0u && g_performance_value < 0x1eu) {
        EF_U16(MOVE_SIDE) = 1;
    } else {
        EF_U16(MOVE_TURN) = turn;
        EF_U16(MOVE_SIDE) = (turn & 1u) == 0u;
    }

    if ((uint16_t)node_has_negative_flag_cc468(MOVE_SIDE_BYTE, context) == 0u)
        return MOVE_NONE;
    if ((uint16_t)find_node_matching_cc51c((uint32_t)(int32_t)(int16_t)EF_U16(MOVE_SIDE), context, 0x15u) == 0u)
        return MOVE_NONE;
    record = find_node_b4db0(head_a, head_b, MOVE_SIDE_BYTE, context);
    group = find_group_node_b4aec(head_a, head_b, MOVE_SIDE_BYTE, context);

    switch (EF_U16(record + 9u)) {
    case 2: case 6: case 11:
        partner = find_sibling_b4c7c(record, MOVE_SIDE_BYTE, head_a, context);
        action = MOVE_PAIR;
        break;
    case 3: case 8: case 12:
        partner = find_child_b4c20(record, MOVE_SIDE_BYTE, head_a, context);
        action = MOVE_PAIR;
        break;
    case 0: case 1: case 4: case 5: case 7: case 9: case 10: case 13: case 14:
        action = MOVE_ONE;
        break;
    case 15: case 16: case 23: case 24:
        if (EF_U16(MOVE_SIDE) != 0u)
            action = MOVE_ONE;
        break;
    default:
        break;
    }
    if (EF_U16(MOVE_SIDE) != 0u && (EF_U16(record + 9u) == 6u || EF_U16(record + 9u) == 8u)) {
        mark_list_entry_64(2, 0, context);
        mark_list_entry_64(0, 2, context);
    } else if (EF_U16(MOVE_SIDE) == 0u && (EF_U16(record + 9u) == 11u || EF_U16(record + 9u) == 12u)) {
        mark_list_entry_9c(3, 1, context);
    }

    if (action == MOVE_SKIP)
        return MOVE_NONE;
    if ((uint16_t)position_in_box_bfb88(record, box, list) == 0u)
        return MOVE_NONE;
    if (action == MOVE_PAIR && (uint16_t)position_in_box_bfb88(partner, box, list) == 0u)
        return MOVE_NONE;

    node = move_record_c0b04(record, list, nodes);
    if (action == MOVE_PAIR) {
        refill_object_cell_c09e4(node, list);
        node = move_record_c0b04(partner, list, nodes);
    }
    turn = EF_U16(MOVE_SIDE);
    refill_object_cell_c09e4(node, list);
    refill_object_cell_c09e4(find_node_by_record_a(group, nodes), list);
    return turn;
}
