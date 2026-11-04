/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Quicksort partition step for 0x15-byte records keyed by their byte at +6. */

/* 0xe6684, 281 bytes. Partition records lo..hi around the middle record's key (descending when the flag is set); returns the pivot's final index. */
uint32_t partition_records_e6684(ef_addr32 base, int16_t lo, int16_t hi, uint32_t descending)
{
    uint32_t temp = guest_fn_cc6c0_x(0x15, 0x00190b50u);
    int16_t mid = (int16_t)(((int32_t)lo + (int32_t)hi) / 2);
    int16_t i = lo;
    int16_t j;
    uint16_t pivot;

    swap_records_e65d8(base + (uint32_t)(int32_t)lo * 0x15u, base + (uint32_t)(int32_t)mid * 0x15u);
    pivot = EF_U8(base + (uint32_t)(int32_t)lo * 0x15u + 6u);
    for (j = (int16_t)(i + 1); j <= hi; j++) {
        uint16_t key = EF_U8(base + (uint32_t)(int32_t)j * 0x15u + 6u);
        int cond = ((uint16_t)descending != 0) ? ((int16_t)key > (int16_t)pivot) : ((int16_t)key < (int16_t)pivot);
        if (cond) {
            i++;
            swap_records_e65d8(base + (uint32_t)(int32_t)i * 0x15u, base + (uint32_t)(int32_t)j * 0x15u);
        }
    }
    swap_records_e65d8(base + (uint32_t)(int32_t)i * 0x15u, base + (uint32_t)(int32_t)lo * 0x15u);
    if (temp != 0)
        guest_fn_cc768(temp);
    return (uint32_t)(int32_t)i;
}
