/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xab28c, 149 bytes. EAX context, six other registers preserved, RET.
 * Sole caller ab5a8 forwards its stream handle. */
void visit_group_nodes_ab28c(ef_addr32 context)
{
    uint32_t group_index = 0u;
    uint32_t item_index;
    ef_addr32 group = EF_U32(0x00419200u);
    ef_addr32 item;
    ef_addr32 record;
    ef_addr32 field;

    while ((int32_t)(int16_t)group_index < (int32_t)EF_U16(0x004194c2u)) {
        item = EF_U32(group + 8u);
        item_index = 0u;
        while ((int32_t)(int16_t)item_index < (int32_t)EF_U16(group + 12u)) {
            if (EF_U8(item) == 14u) {
                record = EF_U32(item + 9u);
                emit_record_pair_ab260(context, EF_U32(record + 0x1eu));
                field = EF_U32(record + 0x22u);
                if (field != 0u)
                    emit_record_pair_ab260(context, field);
                field = EF_U32(record + 0x26u);
                if (field != 0u)
                    emit_record_pair_ab260(context, field);
            }
            item_index++;
            item = EF_U32(item + 1u);
        }
        group_index++;
        group = EF_U32(group);
    }
    read_selector_ab174(context);
}
