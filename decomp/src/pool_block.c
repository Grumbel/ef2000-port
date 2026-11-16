/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
#pragma aux guest_callptr2_x parm [eax] [edx] [ebx] modify [eax ecx edx]
#endif

/* 0xb3db0, 150 bytes. EAX size -> EAX mixed address/handle result;
 * six other registers preserved, plain RET. e74e/bc4f consume signed AX. */
uint32_t take_pool_block_b3db0(uint32_t size)
{
    uint32_t remaining = EF_U32(0x00146e20u) - size;
    uint32_t index;
    uint32_t base;
    uint32_t table;
    uint32_t block;
    uint32_t limit;
    uint16_t next;
    uint16_t handle;

    EF_U32(0x00146e20u) = remaining;
    if ((int32_t)remaining < 32)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018e760u, size);
    index = EF_U16(0x00146e24u);
    base = EF_U32(0x00146e28u);
    remaining = EF_U32(0x00146e20u);
    table = EF_U32(0x0041f370u);
    block = base + remaining;
    EF_U32(table + index * 4u) = block;
    next = (uint16_t)(EF_U16(0x00146e24u) + 1u);
    limit = EF_U32(0x00146e10u);
    EF_U16(0x00146e24u) = next;
    if ((int32_t)(uint32_t)next >= (int32_t)limit)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018e790u);
    fill_bytes_b3fb0(block, 0u, size);
    handle = EF_U16(0x00146e24u);
    handle = (uint16_t)(handle + EF_U16(0x00146e2cu));
    return (block & 0xffff0000u) | (uint32_t)handle;
}
