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
