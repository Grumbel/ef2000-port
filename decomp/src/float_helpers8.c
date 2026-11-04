/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <math.h>

/* Eighth x87 batch: trig leaves and the oriented box hit test. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xb45a2, 10 bytes. fcos on ST0, retried while the FPU reports an incomplete reduction. */
double cos_b45a2(double angle)
{
    return cos(angle);
}

/* 0xb45ac, 10 bytes. fsin on ST0, retried while the FPU reports an incomplete reduction. */
double sin_b45ac(double angle)
{
    return sin(angle);
}

/* 0x7d20, 203 bytes. Test whether a 2D point lies inside the oriented box of a record. */
uint8_t point_in_oriented_box_7d20(ef_addr32 point, ef_addr32 box)
{
    double dx = EF_F64(point) - EF_F64(box + 0x28u);
    double dy = EF_F64(point + 8u) - EF_F64(box + 0x30u);
    double angle = EF_F64(box + 0x18u);
    double s = sin(angle);
    double c = cos(angle);
    int32_t half = EF_S32(box + 0x10u) / 2;
    double u = dx * c + dy * s;
    double v = c * dy - s * dx;

    if (abs_double_b427c(u) < (double)half) {
        half = EF_S32(box + 0x14u) / 2;
        if (abs_double_b427c(v) < (double)half)
            return 1;
    }
    return 0;
}

/* 0x82b8, 219 bytes. Find the first unmarked entry of the cell containing the scaled point and mark it. */
ef_addr32 mark_entry_at_point_82b8(int32_t x, int32_t y)
{
    int32_t cell_x = x >> 19;
    int32_t cell_y = y >> 19;
    double pair[2];
    int32_t i;
    int32_t count = EF_S32(0x001972e4u) >> 16;

    for (i = 0; i < count; i++) {
        ef_addr32 rec = EF_U32(0x001972e0u) + (uint32_t)i * 0x22u;
        uint32_t j;

        if (cell_y != EF_S32(rec + 0xcu) || cell_x != EF_S32(rec + 8u))
            continue;
        pair[0] = (double)y * EF_F64(0x00180410u) * EF_F64(0x00180418u);
        pair[1] = (double)x * EF_F64(0x00180410u) * EF_F64(0x00180418u);
        for (j = 0; j < EF_U8(rec + 0x13u); j++) {
            ef_addr32 entry = EF_U32(rec + 0x16u) + j * 0x38u;

            if (point_in_oriented_box_7d20((ef_addr32)(uintptr_t)pair, entry) != 0) {
                if (EF_U8(entry + 0x27u) == 1)
                    return 0;
                EF_U8(entry + 0x27u) = 1;
                return entry;
            }
        }
    }
    return 0;
}
