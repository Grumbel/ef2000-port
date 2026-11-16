/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* The stack-call shim uses ECX for the target; expose its clobbers here. */
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux guest_callptr2_x parm [eax] [edx] [ebx] modify [eax ecx edx]
#endif

/* 0x98050, 141 bytes. AL slot, all six other registers preserved, RET.
 * Callers 3ed7e/98266 pass zero-extended byte values in EAX. */
void select_sound_slot_98050(uint8_t slot)
{
    ef_addr32 record;

    if (EF_U8(0x00420a72u) == 0u)
        return;
    if (EF_U8(0x00420a70u) != 1u)
        return;
    if (slot >= 4u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018ad0cu, slot);
    /* A returning error callback does not stop the unchecked selection. */
    if (slot != EF_U8(0x0012dd0du)) {
        reset_sound_slot_980e8(slot);
        record = EF_U32(0x0012dd98u) + (uint32_t)slot * 13u;
        if (register_trace_block_d5e0c(record) != 0u) {
            EF_U8(0x00420a70u) = 0u;
            EF_U8(0x00420a72u) = 0u;
            rewrite_status_file_82e40();
        }
    }
    EF_U8(0x0012dd0du) = slot;
    EF_U8(0x0012dd0cu) = 1u;
}
