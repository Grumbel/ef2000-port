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

/* 0x2c600, 218 bytes. For each of a page's eight linked pages (table at 0x1fcb40) other than itself, call 0x46254 with colour 0x3e; if none were drawn and the page is the active one (0x1f71ca) with +0x15 set, fall back to 0x47764. */
void draw_linked_pages_2c600(uint32_t page, uint32_t arg)
{
    int16_t p = (int16_t)page;
    int16_t a = (int16_t)arg;
    uint8_t found = 0u;
    uint32_t rec = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)p * 0xca);
    int16_t i;
    for (i = 0; i < 8; i++) {
        uint32_t list = EF_U32(0x001fcb40u) + (uint32_t)(EF_S16(rec + 0x1cu) * 0x11);
        uint16_t other = EF_U16(list + (uint32_t)i * 2u + 1u);
        if ((int16_t)other != 0xff && other != (uint16_t)p) {
            guest_fn_46254((uint32_t)(int32_t)EF_S16(rec + 0x50u), (int32_t)EF_S16(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)other * 0xca) + 0x50u),
                           0x3eu, (uint32_t)(int32_t)a, 0u, 0u);
            found = 1u;
        }
    }
    if (!found && EF_U16(rec + 0x15u) != 0u && EF_U16(rec + 0x50u) == (uint32_t)EF_S16(0x001f71cau)) {
        guest_fn_47764_x((int32_t)EF_S16(rec + 0x50u), 0x3eu, (int32_t)a, 0u, 0u);
    }
}
