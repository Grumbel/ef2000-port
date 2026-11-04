/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit activity slot stepping. */

/* 0x8654, 242 bytes. Advance a unit's activity slot, nudging the object it refers to. */
void step_activity_slot_8654(uint8_t group, uint8_t slot)
{
    ef_addr32 entry = EF_U32(EF_U32(0x001972e0u) + (uint32_t)group * 0x22u + 0x16u) + (uint32_t)slot * 0x38u;
    ef_addr32 rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(entry + 0x1eu) >> 16) * 0xcau;

    if (EF_U8(rec + 0x20u) == 0) {
        uint8_t kind = EF_U8(rec + 0x1au);
        if (kind != 8 && kind != 9)
            EF_U16(entry + 0x20u) = 0xff;
        return;
    }
    {
        ef_addr32 obj = guest_fn_7489c(EF_S32(rec + (uint32_t)EF_U8(entry + 0x22u) * 2u + 0x4eu) >> 16);
        if (obj != 0 && EF_U8(obj + 0x18du) == 0 && EF_U8(obj + 0x16cu) != 9)
            guest_fn_6a1d0(EF_S32(obj + 0x14u) >> 16, 9);
    }
    entry = EF_U32(EF_U32(0x001972e0u) + (uint32_t)group * 0x22u + 0x16u) + (uint32_t)slot * 0x38u;
    EF_U8(entry + 0x22u) = (uint8_t)(EF_U8(entry + 0x22u) + 1);
    if (EF_U8(entry + 0x22u) > 6)
        EF_U8(entry + 0x22u) = 0;
}
