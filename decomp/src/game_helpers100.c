/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 3D bounds, table entry fill and angle-weighted blend. */

/* 0xd2040, 205 bytes. Compute (once) the 3D bounding box of an object's points into fields 0x30..0x44. */
void compute_point_bounds_d2040(ef_addr32 object)
{
    ef_addr32 list;
    int32_t n;
    int32_t i;
    int32_t min_x = 0x7fffffff;
    int32_t min_y = 0x7fffffff;
    int32_t min_z = 0x7fffffff;
    int32_t max_x = (int32_t)0x80000000;
    int32_t max_y = (int32_t)0x80000000;
    int32_t max_z = (int32_t)0x80000000;

    if ((EF_U8(object + 0x2cu) & 4u) != 0)
        return;
    list = EF_U32(object + 0x4cu);
    n = EF_S32(list);
    for (i = 0; i < n; i++) {
        ef_addr32 pt = EF_U32(list + 4u + (uint32_t)i * 4u);
        int32_t x = EF_S32(pt);
        int32_t y = EF_S32(pt + 4u);
        int32_t z = EF_S32(pt + 8u);
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (z < min_z) min_z = z;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
        if (z > max_z) max_z = z;
    }
    EF_S32(object + 0x30u) = min_x;
    EF_S32(object + 0x3cu) = max_x;
    EF_S32(object + 0x34u) = min_y;
    EF_S32(object + 0x38u) = min_z;
    EF_S32(object + 0x40u) = max_y;
    EF_S32(object + 0x44u) = max_z;
    EF_U8(object + 0x2cu) = (uint8_t)(EF_U8(object + 0x2cu) | 4u);
}

/* 0xb570c, 242 bytes. Fill the current (last added) 0x19-byte table entry of an object from a source record and a flag byte. */
void fill_current_table_entry_b570c(ef_addr32 object, ef_addr32 source, uint8_t flag)
{
    int32_t idx = (EF_S32(object + 0x1au) >> 16) - 1;
    ef_addr32 e = EF_U32(object + 0x20u) + (uint32_t)idx * 0x19u;

    EF_U8(e + 0x18u) = flag;
    EF_U16(e + 0x16u) = 0;
    EF_U16(e + 0xcu) = 0;
    EF_U32(e) = EF_U32(source + 0x11u);
    EF_U32(e + 4u) = EF_U32(source + 0x15u);
    EF_U16(e + 8u) = EF_U16(source + 0x1du);
    EF_U16(e + 0xau) = EF_U16(source + 0x1fu);
}

/* 0xa2244, 240 bytes. Blend two sets of three words into rec+8..0xc, weighted by the cosine of the record's angle (1.14 weights). */
void blend_by_angle_weight_a2244(ef_addr32 rec)
{
    int32_t angle = EF_S32(rec + 2u) >> 16;
    uint32_t idx = ((uint32_t)angle & 0xffffu) >> 3;
    int32_t w = ((int32_t)EF_S16(0x00141e08u + idx * 2u) * 0x4000) / 0x8000;
    int32_t a = (int16_t)(0x4000 - w);
    int32_t b = (int16_t)w;
    uint32_t k;

    for (k = 0; k < 3; k++) {
        int32_t first = ((int32_t)EF_S16(rec + 0xeu + k * 2u) * a) / 0x4000;
        int32_t second = ((int32_t)EF_S16(rec + 0x14u + k * 2u) * b) / 0x4000;

        EF_U16(rec + 8u + k * 2u) = (uint16_t)(first + second);
    }
}
