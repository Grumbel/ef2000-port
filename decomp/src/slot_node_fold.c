/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* The slot record kind used for the node kinds 0xf, 0x10 and 0x11. Other
 * kinds leave the previous value; the original then reads a stale stack
 * byte, which the callers never reach. */
static uint8_t slot_kind_for(uint8_t kind, uint8_t previous)
{
    switch (kind) {
    case 0x0f:
        return 5u;
    case 0x10:
        return 0x13u;
    case 0x11:
        return 0x12u;
    default:
        return previous;
    }
}

/* 0xc03b4, 382 bytes (6 stack arguments, ret 0x18). Fold the first
 * matching node of the node list at HEAD into the running value CUR.
 * A node matches when the negative group test 0xcc1ac passes for J and
 * its group byte (+0xb) in LIST, and its kind byte (+0xa) is KIND. For a
 * matching node a slot record of the kind's slot kind is created and
 * tested with 0xbdd64 against the node's target (pointer at +0x13,
 * dwords +8 and +0xc); a non-zero result is folded into CUR by 0xbe59c.
 * The record is freed after each try. Trying stops for the rest of the
 * call after one fold or after the first failed test; the group test
 * still runs for every node. Returns CUR. */
uint16_t fold_slot_nodes_c03b4(int16_t cur, uint8_t kind, uint8_t j, ef_addr32 list, uint32_t p0, uint32_t p1,
                               ef_addr32 head, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int16_t folds = 0;
    uint16_t trying = 1u;
    uint8_t slot_kind = 0u;
    ef_addr32 node;

    for (node = EF_U32(head); node != 0u; node = EF_U32(node + 4u)) {
        if ((uint16_t)group_flag_negative_cc1ac(j, EF_U8(node + 0xbu), list) == 0u ||
            EF_U8(node + 0xau) != kind)
            continue;
        while (folds < 1 && trying != 0u) {
            ef_addr32 record;
            ef_addr32 target = EF_U32(node + 0x13u);
            uint32_t found;

            slot_kind = slot_kind_for(kind, slot_kind);
            record = create_slot_record_fc0ec(j, slot_kind);
            trying = 1u;
            found = guest_fn_bdd64_x(record, p0, p1, EF_U32(target + 8u), EF_U32(target + 0xcu),
                                     (uint32_t)(int32_t)cur, p4, list, head);
            if (found != 0u) {
                cur = (int16_t)guest_fn_be59c(record, 0u, node, found, (uint32_t)(int32_t)cur, 0u, 1u, head, p3,
                                              p5, p4, p0, p1, 0u, list);
                ++folds;
            } else {
                trying = 0u;
            }
            free_entry_array_b16c0(record);
        }
    }
    return (uint16_t)cur;
}
