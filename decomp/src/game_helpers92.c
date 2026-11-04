/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Recursive range sort driver and depth-extent test. */

/* 0xe67a0, 93 bytes. Quicksort driver: partition [lo,hi] with e6684, recurse on the left part and loop on the right. */
void quicksort_range_e67a0(ef_addr32 object, int16_t lo, int16_t hi, int16_t extra)
{
    while (lo < hi) {
        int16_t p = (int16_t)guest_fn_e6684_x(object, (int32_t)lo, (int32_t)hi, (int32_t)extra);

        quicksort_range_e67a0(object, lo, (int16_t)(p - 1), extra);
        lo = (int16_t)(p + 1);
    }
}

/* 0x10162c, 91 bytes. Return 1 when the minimum and maximum of the vertices' z field have the same integer part (high 16 bits). */
uint8_t vertex_depth_range_flat_10162c(ef_addr32 object)
{
    ef_addr32 list = EF_U32(object + 0x4cu);
    int32_t count = EF_S32(list);
    int32_t min_z = 0x7fffffff;
    int32_t max_z = (int32_t)0x80000000;
    int32_t i;

    for (i = 0; i < count; i++) {
        int32_t z = EF_S32(EF_U32(list + 4u + (uint32_t)i * 4u) + 8u);
        if (z < min_z)
            min_z = z;
        if (z > max_z)
            max_z = z;
    }
    return (uint8_t)((uint16_t)((uint32_t)min_z >> 16) == (uint16_t)((uint32_t)max_z >> 16));
}
