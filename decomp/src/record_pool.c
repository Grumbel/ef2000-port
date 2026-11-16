/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb0138,208 bytes. EAX list pointer, EDX count pointer -> EAX pool;
 * EDX scratch, five other registers preserved, RET. Sole caller 49461. */
ef_addr32 alloc_record_pool_b0138(ef_addr32 listp, ef_addr32 countp)
{
    ef_addr32 node = EF_U32(listp);
    ef_addr32 entry, pool, cursor;
    uint32_t index, count, bytes;

    EF_U32(countp) = 0u;
    while (node != 0u) {
        index = 0u;
        while ((int32_t)(int16_t)index < (int32_t)EF_U8(node + 0x12u)) {
            entry = EF_U32(node + 0x13u)
                + (uint32_t)(int32_t)(int16_t)index * 124u;
            count = (uint32_t)EF_U8(entry + 1u) * 20u;
            count += EF_U32(countp);
            index++;
            EF_U32(countp) = count;
        }
        node = EF_U32(node + 4u);
    }
    if (EF_U32(countp) == 0u)
        EF_U32(countp) = 2000u;
    count = EF_U32(countp);
    bytes = count * 54u;
    pool = guest_alloc_fmt_cc6c0(bytes, 0x0018dd7cu, bytes, count);
    bytes = EF_U32(countp) * 54u;
    fill_bytes_b3fb0(pool, 0u, bytes);
    index = 0u;
    if (EF_S32(countp) > 0) {
        cursor = pool;
        do {
            EF_U16(cursor + 0x30u) = 0xffffu;
            EF_U16(cursor + 0x32u) = 0xffffu;
            EF_U16(cursor + 0x34u) = 0xffffu;
            index++;
            count = EF_U32(countp);
            cursor += 54u;
        } while ((int32_t)index < (int32_t)count);
    }
    EF_U32(0x001379b0u) = 0u;
    EF_U16(0x001379b8u) = 0u;
    return pool;
}
