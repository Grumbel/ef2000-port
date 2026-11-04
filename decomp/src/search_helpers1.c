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

/* 0x3170c, 103 bytes. Rotate the vector at 0x19e57e by zero angles into 0x19e5c1 and add the offset triple at 0x19e560. */
void place_rotated_vector_3170c(void)
{
    int16_t angles[3] = { 0, 0, 0 };
    rotate_vector_by_angles_32414((uint32_t)(uintptr_t)angles, 0x0019e57eu, 0x0019e5c1u);
    EF_U32(0x0019e5c1u) = EF_U32(0x0019e5c1u) + EF_U32(0x0019e560u);
    EF_U32(0x0019e5c5u) = EF_U32(0x0019e5c5u) + EF_U32(0x0019e564u);
    EF_U32(0x0019e5c9u) = EF_U32(0x0019e5c9u) + EF_U32(0x0019e568u);
}

/* 0x85380, 104 bytes. While the owner record has an active chain (byte +0x8e) whose last 0x19-byte entry has kind 2, run 0x85358. */
void drain_kind_two_chain_85380(uint32_t owner)
{
    int16_t o = (int16_t)owner;
    for (;;) {
        uint32_t rec = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)o * 0xca);
        uint32_t n = EF_U8(rec + 0x8eu);
        if (n == 0u)
            break;
        if (EF_U8(EF_U32(rec + 0x8au) + (n - 1u) * 0x19u + 0x18u) != 2u)
            break;
        decrement_grid_activity((uint32_t)(int32_t)o);
    }
}
