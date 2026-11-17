/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xe9a24, 132 bytes. Stack arguments: count, source, destination; RET 12.
 * Return the advanced destination. Load each 16/8-byte group completely
 * before storing it: overlapping regions deliberately do not act as memmove.
 * Signed branch predicates include the original ADD overflow behavior. */
ef_addr32 copy_blocks_e9a24(uint32_t count, ef_addr32 source,
                           ef_addr32 destination)
{
    uint32_t remaining = count - 32u;
    uint32_t before = count;
    uint32_t a, b, c, d;
    if ((int32_t)before >= 32) {
        do {
            a = EF_U32(source);
            b = EF_U32(source + 4u);
            c = EF_U32(source + 8u);
            d = EF_U32(source + 12u);
            EF_U32(destination) = a;
            EF_U32(destination + 4u) = b;
            EF_U32(destination + 8u) = c;
            EF_U32(destination + 12u) = d;
            source += 32u;
            destination += 32u;
            a = EF_U32(source - 16u);
            b = EF_U32(source - 12u);
            c = EF_U32(source - 8u);
            d = EF_U32(source - 4u);
            EF_U32(destination - 16u) = a;
            EF_U32(destination - 12u) = b;
            EF_U32(destination - 8u) = c;
            EF_U32(destination - 4u) = d;
            before = remaining;
            remaining -= 32u;
        } while ((int32_t)before >= 32);
    }
    if ((int32_t)remaining > -32) {
        remaining += 32u;
        before = remaining;
        remaining -= 8u;
        if ((int32_t)before >= 8) {
            do {
                a = EF_U32(source);
                b = EF_U32(source + 4u);
                EF_U32(destination) = a;
                EF_U32(destination + 4u) = b;
                source += 8u;
                destination += 8u;
                before = remaining;
                remaining -= 8u;
            } while ((int32_t)before >= 8);
        }
        if ((int32_t)remaining > -8) {
            remaining += 8u;
            do {
                EF_U8(destination) = EF_U8(source);
                ++source;
                ++destination;
            } while (--remaining != 0u);
        }
    }
    return destination;
}
