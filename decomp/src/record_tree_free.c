/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/record_tree.h"

/* 0xb1a68, 121 bytes. EAX record; all other registers preserved, RET.
 * Sole caller b1af5 keeps its next-list pointer in EDX across this call. */
void free_record_tree_b1a68(ef_addr32 rec)
{
    uint32_t group_index = 0u;
    uint32_t entry_index;
    ef_addr32 group;
    ef_addr32 allocation;

    if (rec == 0u)
        return;
    if (EF_S16(rec + 0x1eu) > 0) {
        do {
            entry_index = 0u;
            for (;;) {
                group = EF_U32(rec + 0x20u)
                    + (uint32_t)(int32_t)(int16_t)group_index * 10u;
                if ((int16_t)entry_index >= EF_S16(group + 4u))
                    break;
                allocation = EF_U32(EF_U32(group + 6u)
                    + (uint32_t)(int32_t)(int16_t)entry_index * 6u + 2u);
                if (allocation != 0u)
                    guest_fn_cc768(allocation);
                entry_index++;
            }
            allocation = EF_U32(group + 6u);
            if (allocation != 0u)
                guest_fn_cc768(allocation);
            group_index++;
        } while ((int16_t)group_index < EF_S16(rec + 0x1eu));
    }
    allocation = EF_U32(rec + 0x20u);
    if (allocation != 0u)
        guest_fn_cc768(allocation);
    guest_fn_cc768(rec);
}
