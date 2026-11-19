/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_RECORD_POOL_H
#define EF_STRUCTS_RECORD_POOL_H

#include "ef_types.h"

/* List node walked by the pool sizers at 0xb00d0 and 0xb0138. Only the
 * fields those two functions read are recovered. */
typedef struct EF_PACKED ef_pool_source_node {
    uint8_t unknown_00[4];
    ef_addr32 next;                  /* +0x04, zero ends the list */
    uint8_t unknown_08;
    uint8_t kind;                    /* +0x09, 0xb00d0 counts kinds 0 and 1 */
    uint8_t unknown_0a[8];
    uint8_t group_count;             /* +0x12 */
    ef_addr32 groups;                /* +0x13, 124-byte group records */
} ef_pool_source_node;
EF_STATIC_ASSERT(ef_pool_source_next_off, offsetof(ef_pool_source_node, next) == 4);
EF_STATIC_ASSERT(ef_pool_source_kind_off, offsetof(ef_pool_source_node, kind) == 9);
EF_STATIC_ASSERT(ef_pool_source_count_off, offsetof(ef_pool_source_node, group_count) == 0x12);
EF_STATIC_ASSERT(ef_pool_source_groups_off, offsetof(ef_pool_source_node, groups) == 0x13);

#endif
