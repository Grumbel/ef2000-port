/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa7510, 119 bytes. EAX widget, EDX text; plain RET.
 * EBX/ECX/ESI/EDI/EBP preserved; EDX advances in the final copy.
 * Sole caller a758f overwrites EDX before relayout. */
void set_record_name_a7510(ef_addr32 record, ef_addr32 name)
{
    uint32_t length, capacity;
    ef_addr32 buffer;

    if (name == 0u)
        name = 0x0018d1f4u;
    if (EF_U32(record + 0x27u) == 0u) {
        length = strlen_ds(name);
        EF_U16(record + 0x3du) = (uint16_t)length;
        /* Allocation uses the stored low word, not the full length. */
        capacity = EF_U16(record + 0x3du);
        buffer = alloc_zeroed_ab840(capacity + 1u);
        EF_U32(record + 0x27u) = buffer;
        if (buffer == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d1f8u, name);
    } else {
        capacity = EF_U16(record + 0x3du);
        length = strlen_ds(name);
        if (length > capacity) {
            buffer = EF_U32(record + 0x27u);
            guest_callptr4c_x(EF_U32(0x00132a74u), 0x0018d234u,
                             record + 0x3fu, name, buffer);
        }
    }
    /* ORIGINAL BUG: an error callback that returns still permits the
     * unchecked copy, including after failed allocation or overflow. */
    guest_fn_9d749(EF_U32(record + 0x27u), name);
}
