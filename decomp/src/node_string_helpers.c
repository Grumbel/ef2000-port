/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb1b8b, 25 bytes. strlen over guest memory (repnz scasb, ES=DS). */
uint32_t strlen_ds(ef_addr32 s)
{
    uint32_t n = 0;

    while (EF_U8(s + n) != 0)
        n++;
    return n;
}

/* 0xb0208, 47 bytes. Walks the singly linked list whose head pointer is
 * stored at head_addr (next link at +4). Returns 1 when a node has
 * byte[+0xb] == a, byte[+0xa] == b and byte[+0xc] != 1. */
uint32_t list_has_matching_node(ef_addr32 head_addr, uint8_t a, uint8_t b)
{
    ef_addr32 node = EF_U32(head_addr);

    while (node != 0) {
        if (EF_U8(node + 0xb) == a && EF_U8(node + 0xa) == b &&
            EF_U8(node + 0xc) != 1)
            return 1;
        node = EF_U32(node + 4);
    }
    return 0;
}

/* 0xb0850, 260 bytes. Counts list nodes (head at [list_addr], next at +4)
 * whose flag test passes and whose type (signed 16.16 high half at +7) is in
 * the set selected by kind (0: 1-6,8,9,11-14; 1: 0x11-0x16,0x18-0x1a). */
uint32_t count_filtered_node_matches(uint8_t kind, ef_addr32 list_addr, ef_addr32 aux)
{
    uint32_t count = 0;
    ef_addr32 node = EF_U32(list_addr);

    if (node == 0) {
        return 0;
    }
    if (kind > 1) {
        return 0;
    }
    while (node != 0) {
        if ((uint16_t)guest_fn_cc1ac(kind, EF_U8(node + 0xb), aux) != 0) {
            int32_t type = (int32_t)EF_U32(node + 7) >> 16;

            if (kind == 0) {
                if ((type >= 1 && type <= 6) || type == 8 || type == 9 ||
                    (type >= 11 && type <= 14)) {
                    count++;
                }
            } else {
                if ((type >= 0x11 && type <= 0x16) || (type >= 0x18 && type <= 0x1a)) {
                    count++;
                }
            }
        }
        node = EF_U32(node + 4);
    }
    return count;
}

/* 0xb0954, 427 bytes. Like count_filtered_node_matches but uses the
 * positive-flag test (guest_fn_cc22c) and a wider type set: nodes whose
 * word at +9 is zero always count; otherwise kind 0 counts types 1-16, 23
 * and 24, kind 1 counts types 7, 10, 15, 16 and 0x11-0x1a. */
uint32_t count_positive_filtered_node_matches(uint8_t kind, ef_addr32 list_addr, ef_addr32 aux)
{
    uint32_t count = 0;
    ef_addr32 node = EF_U32(list_addr);

    if (node == 0) {
        return 0;
    }
    if (kind > 1) {
        return 0;
    }
    while (node != 0) {
        if ((uint16_t)guest_fn_cc22c(kind, EF_U8(node + 0xb), aux) != 0) {
            int32_t type = (int32_t)EF_U32(node + 7) >> 16;
            int match = EF_U16(node + 9) == 0;

            if (kind == 0) {
                if ((type >= 1 && type <= 16) || type == 0x17 || type == 0x18) {
                    match = 1;
                }
            } else {
                if (type == 7 || type == 10 || type == 15 || type == 16 ||
                    (type >= 0x11 && type <= 0x1a)) {
                    match = 1;
                }
            }
            if (match) {
                count++;
            }
        }
        node = EF_U32(node + 4);
    }
    return count;
}
