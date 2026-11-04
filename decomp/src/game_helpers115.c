/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Word array shuffle. */

/* 0xb0cfc, 245 bytes. Shuffle pass over a word array: for each iteration pick two random positions and swap them with the xor trick. */
/* ORIGINAL BUG: both positions are chosen as n-1 whenever the random index is below n-1 (always), so the xor swap zeroes the last element instead of shuffling. */
void shuffle_words_b0cfc(ef_addr32 array, int16_t n, uint8_t start, int16_t limit)
{
    int32_t i = (int32_t)start + 1;

    while ((int16_t)i < limit) {
        int32_t last = (int32_t)(int16_t)n - 1;
        int32_t span = last - (int32_t)start;
        int32_t a;
        int32_t b;
        ef_addr32 pa;
        ef_addr32 pb;
        uint16_t t;
        uint16_t u;

        a = (int32_t)((uint16_t)guest_fn_a45b3() % span) + start;
        if (last > a)
            a = last;
        else
            a = (int32_t)start + (int32_t)((uint16_t)guest_fn_a45b3() % span);
        b = (int32_t)((uint16_t)guest_fn_a45b3() % span) + start;
        if (last > b)
            b = last;
        else
            b = (int32_t)start + (int32_t)((uint16_t)guest_fn_a45b3() % span);
        pa = array + (uint32_t)(int32_t)(int16_t)a * 2u;
        pb = array + (uint32_t)(int32_t)(int16_t)b * 2u;
        t = (uint16_t)(EF_U16(pa) ^ EF_U16(pb));
        EF_U16(pa) = t;
        u = (uint16_t)(EF_U16(pb) ^ t);
        EF_U16(pb) = u;
        EF_U16(pa) = (uint16_t)(EF_U16(pa) ^ u);
        i++;
    }
}
