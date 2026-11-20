/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Day counter, advanced when the 8-step clock at 0x1379a0 wraps (0xb0404). */
#define CAMPAIGN_DAY 0x0013799eu

/* 0xbc610, 475 bytes. Run the node passes over LIST for VALUE, threading
   the 16-bit accumulator through each pass. On odd days after day 3 the
   0xc1034 object pass runs before the 0xbf444 pass and after it on other
   days. Returns the accumulator from the last pass. */
uint16_t run_node_passes_bc610(int16_t value, ef_addr32 list, uint32_t arg_b, uint32_t arg_c,
                               uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5)
{
    int32_t day = EF_S16(CAMPAIGN_DAY);
    int objects_first = day > 3 && day % 2 != 0;
    int16_t acc;

    acc = (int16_t)scan_nodes_bfa58((uint16_t)value, list, arg_b, arg_c, s1, s2, s5);
    acc = (int16_t)process_pending_nodes_bf81c((uint16_t)acc, list, arg_b, arg_c, s1, s2, s3, 0xf, s5);
    if (!objects_first)
        acc = (int16_t)guest_fn_bf444(acc, list, arg_b, arg_c, s1, s2, s5);
    else
        acc = (int16_t)loop_objects_c1034(acc, s4, s5, list, arg_b, arg_c, s1, s2, s3);
    acc = (int16_t)process_pending_nodes_bf81c((uint16_t)acc, list, arg_b, arg_c, s1, s2, s3, 0x10, s5);
    acc = (int16_t)process_pending_nodes_bf81c((uint16_t)acc, list, arg_b, arg_c, s1, s2, s3, 0x11, s5);
    acc = (int16_t)fold_list_kinds_c02ec((uint32_t)(int32_t)acc, s5, list, arg_b, arg_c, s1, s2, s3);
    acc = (int16_t)fold_list_kinds_c0534((uint32_t)(int32_t)acc, s5, list, arg_b, arg_c, s1, s2, s3);
    acc = (int16_t)for_each_entry_beebc(s4, acc, s5, list, arg_b, arg_c, s1, s2, s3);
    if (!objects_first)
        return (uint16_t)loop_objects_c1034(acc, s4, s5, list, arg_b, arg_c, s1, s2, s3);
    return (uint16_t)guest_fn_bf444(acc, list, arg_b, arg_c, s1, s2, s5);
}
