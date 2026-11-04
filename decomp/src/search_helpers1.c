/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Text search and cell tests. */

/* 0xe0b10, 92 bytes. Search the buffer at 0x15f5fd (length byte at 0x15f684) for the text, ignoring case; on a hit remember its position at 0x15f68c, set 0x15f695 and return 1. */
uint32_t find_text_in_buffer_e0b10(ef_addr32 text)
{
    uint32_t len, pos, i;
    int32_t limit;
    if (EF_U8(0x0015f684u) == 0u)
        return 0u;
    len = guest_fn_b1b8b(text);
    pos = 0x0015f5fdu;
    limit = (int32_t)EF_U8(0x0015f684u) - (int32_t)len;
    for (i = 0; (int32_t)i <= limit; i++, pos++) {
        if (compare_nocase_10901d(pos, text, len) == 0) {
            EF_U32(0x0015f68cu) = pos;
            EF_U8(0x0015f695u) = 1u;
            return 1u;
        }
    }
    return 0u;
}

/* 0x3a268, 94 bytes. Return 1 when the cell found for the 69-byte record's coordinates (dwords at +7/+0xf divided by 2048) differs from the record's expected cell (word +0x1d). */
uint32_t cell_changed_3a268(uint32_t index)
{
    uint32_t rec = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)(int16_t)index * 69);
    int16_t pt[2];
    uint32_t cell;
    pt[0] = (int16_t)((int32_t)EF_S32(rec + 0xfu) / 2048);
    pt[1] = (int16_t)((int32_t)EF_S32(rec + 7u) / 2048);
    cell = guest_fn_85bc8_x((uint32_t)(uintptr_t)pt) & 0xffu;
    return cell != EF_U16(rec + 0x1du);
}
