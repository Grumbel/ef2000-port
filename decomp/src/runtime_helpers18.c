/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Allocation and nested list lookup helpers, 0xaafe8..0xab840. */

/* 0xaafe8, 120 bytes. Find a node in the nested group lists; report its index and group index. */
uint32_t find_node_position_aafe8(ef_addr32 out_index, ef_addr32 out_group, ef_addr32 target)
{
    ef_addr32 group = EF_U32(0x00419200u);
    int16_t group_index = 0;

    while ((int32_t)group_index < (int32_t)EF_U16(0x004194c2u)) {
        ef_addr32 node = EF_U32(group + 8u);
        int16_t i = 0;

        while ((int32_t)i < (int32_t)EF_U16(group + 0xcu)) {
            if (target == node) {
                EF_U16(out_index) = (uint16_t)i;
                EF_U16(out_group) = (uint16_t)group_index;
                return 1;
            }
            i++;
            node = EF_U32(node + 1u);
        }
        group_index++;
        group = EF_U32(group);
    }
    return 0;
}

/* 0xab840, 111 bytes. Bump-allocate a zeroed block from the arena; 0 when it does not fit. */
uint32_t alloc_zeroed_ab840(uint32_t size)
{
    uint32_t top = EF_U32(0x00137270u);
    uint32_t base;

    if (size == 0) {
        base = top;
    } else if (top + size > EF_U32(0x00137274u)) {
        base = 0;
    } else {
        guest_fn_b3fb0(top, 0, size);
        top = EF_U32(0x00137270u) + size;
        if (top - EF_U32(0x0013726cu) > EF_U32(0x00137278u))
            EF_U32(0x00137278u) = top - EF_U32(0x0013726cu);
        base = top - size;
    }
    EF_U32(0x00137270u) = top;
    return base;
}
