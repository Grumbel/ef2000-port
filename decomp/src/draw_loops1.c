/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Repeated draw calls. */

/* 0x461e8, 106 bytes. Call 0x46254 for each of the seven 16-bit entries (+0x4e) of a 0xca-byte record. */
void draw_record_entries_461e8(uint32_t x, uint32_t index, uint32_t y, uint32_t c, uint32_t s0, uint32_t s1)
{
    uint32_t i;
    uint32_t rec = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)index * 0xca);
    for (i = 0; i < 7u; i++) {
        guest_fn_46254((uint32_t)(int32_t)(int16_t)x,
                       (int32_t)EF_S16(rec + 0x50u + i * 2u),
                       (uint32_t)(int32_t)(int16_t)y, c, s0, s1);
    }
}

/* 0x46168, 128 bytes. For the object found by guest_fn_7489c, call 0x46254 for each of its page record's seven 16-bit entries (+0x50) that differ from the key. */
void draw_object_entries_46168(uint32_t key, uint32_t b, uint32_t c, uint32_t d, uint32_t s0)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)(int16_t)key);
    uint32_t i;
    if (obj == 0u)
        return;
    for (i = 0; i < 7u; i++) {
        uint32_t rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S16(obj + 0x28u) * 0xca) + i * 2u;
        if (EF_U16(rec + 0x50u) != (uint32_t)(int32_t)(int16_t)key) {
            guest_fn_46254((uint32_t)(int32_t)(int16_t)key,
                           (int32_t)EF_S16(rec + 0x50u),
                           (uint32_t)(int32_t)(int16_t)b, c, d, s0);
        }
    }
}
