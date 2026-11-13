/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0xaa198, 39 bytes. Replace a signed index at unaligned record+1
 * with a node from the list. Nonpositive indices select the first node. */
void resolve_link_at_1(ef_addr32 record)
{
    ef_addr32 node = EF_U32(0x4191fcu);
    int32_t i = 0;
    while (i < EF_S32(record + 1u)) {
        node = EF_U32(node);
        ++i;
    }
    EF_U32(record + 1u) = node;
}

/* 0xaa1c0, 36 bytes. The same operation on the dword at record+0. */
void resolve_link_at_0(ef_addr32 record)
{
    ef_addr32 node = EF_U32(0x4191fcu);
    int32_t i = 0;
    while (i < EF_S32(record)) {
        node = EF_U32(node);
        ++i;
    }
    EF_U32(record) = node;
}
