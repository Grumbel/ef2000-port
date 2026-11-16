/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb29a8, 129 bytes. EAX list, EDX output; EDX/EAX scratch,
 * EBX/ECX/ESI/EDI/EBP preserved, plain RET. Callers b26d2/b26ff. */
void dump_group_list_b29a8(ef_addr32 list, uint32_t out)
{
    uint32_t group_index = 0u;
    uint32_t item_index;
    ef_addr32 group;
    int32_t count;
    ef_addr32 name;

    count = EF_S16(list + 1u);
    name = EF_U32(0x00132cd0u + (uint32_t)EF_U8(list) * 4u);
    print_to_handle_b1bb0(out, 0x0018e2a4u, name, count);
    if (EF_S16(list + 1u) > 0) {
        do {
            group = EF_U32(list + 3u)
                + (uint32_t)(int32_t)(int16_t)group_index * 6u;
            print_to_handle_b1bb0(out, 0x0018e2bcu, (int32_t)EF_S16(group));
            item_index = 0u;
            for (;;) {
                group = EF_U32(list + 3u)
                    + (uint32_t)(int32_t)(int16_t)group_index * 6u;
                if ((int16_t)item_index >= EF_S16(group))
                    break;
                print_class_record_b2960(EF_U32(group + 2u)
                    + (uint32_t)(int32_t)(int16_t)item_index * 17u, out);
                item_index++;
            }
            group_index++;
        } while ((int16_t)group_index < EF_S16(list + 1u));
    }
}
