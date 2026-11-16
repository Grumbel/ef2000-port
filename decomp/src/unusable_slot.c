/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/slot_table.h"

/* 0xbd5f8, 144 bytes. EAX record, EAX boolean result; six preserved
 * registers, plain RET. Callers bdc8d/bdcd7 test AX, not just AL. */
uint32_t clear_unusable_slots_bd5f8(ef_addr32 rec)
{
    uint32_t kind = EF_U8(rec + 1u);
    int32_t index;
    uint32_t offset;
    ef_addr32 slot;

    if (kind == 2u)
        return 0u;
    if (kind == 25u && (int16_t)count_nonzero_entries(rec) < 2)
        return 0u;
    index = (int32_t)EF_U8(rec + 5u) - 1;
    while (index >= 0) {
        offset = (uint32_t)index * 10u;
        slot = EF_U32(rec + 6u) + offset;
        if (EF_U16(slot) != 0u
            && (uint16_t)slot_usable_bd5a8(rec, (int16_t)index) == 0u) {
            /* Reload table after the predicate, retaining the old offset.
             * Only the first unusable slot in reverse order is cleared. */
            EF_U16(EF_U32(rec + 6u) + offset) = 0u;
            return 1u;
        }
        index--;
    }
    return 0u;
}
