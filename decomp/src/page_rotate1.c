/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Page transforms. */

/* 0xe8368, 112 bytes. Look up a page's vertex group and rotate each 0x4a-byte entry's three points, storing the rotation parameters at 0x464370..0x46437c. */
void rotate_page_entries_e8368(uint32_t page, uint32_t p0, uint32_t p1, uint32_t p2)
{
    uint32_t key = lookup_name_value_a1a40(0x00190c68u);
    uint32_t group, rec;
    int32_t n;
    if (key == 0u)
        return;
    group = find_page_pair_a1a90((int16_t)page, (int32_t)key);
    if (group == 0u)
        return;
    EF_U32(0x00464370u) = p0;
    EF_U32(0x00464378u) = p2;
    rec = group + 2u;
    n = EF_S16(group);
    EF_U32(0x00464374u) = p1;
    EF_U8(0x0046437cu) = record_flag_bits56((int16_t)page);
    while (n > 0) {
        rotate_calls_e8348(rec, rec + 6u, rec + 0xcu);
        n--;
        rec += 0x4au;
    }
}
