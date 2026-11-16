/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Allocator targets 984f0/9850c consume EAX only and preserve EDX.
 * No extra flags argument is present in either original call. */

/* 0x39db0, 20 bytes. AL count; six registers preserved, plain RET.
 * Sole caller 39dd4 passes ten. */
void alloc_slot_table_39db0(uint8_t count)
{
    EF_U32(0x001a1860u) = ((ef_addr32 (*)(uint32_t))EF_U32(0x0012dd9cu))(
        (uint32_t)count * 4u);
}

/* 0x8204c, 18 bytes. EAX value -> EAX allocation; six other registers
 * preserved, plain RET. Sole caller 4b0a5 stores returned pointer. */
ef_addr32 make_node_8204c(uint32_t value)
{
    ef_addr32 node = ((ef_addr32 (*)(uint32_t))EF_U32(0x0012dd9cu))(12u);
    EF_U32(node) = value;
    return node;
}
