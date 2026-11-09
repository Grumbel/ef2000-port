/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9d524, 57 bytes. ASCII-only case-insensitive comparison. The original
 * compares folded bytes, then returns their signed difference in EAX. */
int32_t ascii_stricmp_9d524(ef_addr32 left, ef_addr32 right)
{
    for (;;) {
        uint32_t a = EF_U8(left);
        uint32_t b = EF_U8(right);

        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z')
            b += 'a' - 'A';
        if (a != b || b == 0)
            return (int32_t)a - (int32_t)b;
        left++;
        right++;
    }
}
