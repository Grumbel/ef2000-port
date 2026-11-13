/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0x10d5c7, 116 bytes. Insert a heap block into the address-sorted list at 0x16464c and reset it. */
ef_addr32 insert_heap_block_10d5c7(ef_addr32 block)
{
    ef_addr32 next = EF_U32(0x0016464cu);
    ef_addr32 prev = 0;
    uint32_t size;
    ef_addr32 body;

    while (next != 0 && block >= next) {
        prev = next;
        next = EF_U32(next + 8u);
    }
    EF_U32(block + 4u) = prev;
    EF_U32(block + 8u) = next;
    if (prev != 0)
        EF_U32(prev + 8u) = block;
    else
        EF_U32(0x0016464cu) = block;
    if (next != 0)
        EF_U32(next + 4u) = block;

    size = EF_U32(block);
    body = block + 0x2cu;
    EF_U32(block + 0x20u) = 0;
    EF_U32(block + 0x10u) = 0;
    EF_U32(block + 0x18u) = 0;
    EF_U32(block + 0x1cu) = 0;
    EF_U32(block + 0x24u) = block + 0x20u;
    EF_U32(block + 0x28u) = block + 0x20u;
    EF_U32(block + 0xcu) = block + 0x20u;
    size -= 0x2cu;
    EF_U32(body) = size;
    EF_U32(body + size) = 0xffffffffu;
    return body;
}
