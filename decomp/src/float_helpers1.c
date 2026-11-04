/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* First x87 batch: vector lengths, scaled offsets and double table lookups. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x3c74, 96 bytes. Integer length of a 3D integer vector, at least 1. */
int32_t vector_length_3c74(int32_t x, int32_t y, int32_t z)
{
    double sum = (double)x * (double)x;

    sum = (double)y * (double)y + sum;
    sum = (double)z * (double)z + sum;
    if (1.0 > sum)
        return 1;
    return (int32_t)guest_fn_a4470(sum);
}

/* 0x66c4, 56 bytes. Integer result of a table lookup for the current object, or 0. */
int32_t current_object_metric_66c4(void)
{
    int32_t result = 0;
    ef_addr32 object = EF_U32(0x001a821cu);

    if (object != 0)
        result = (int32_t)guest_fn_6a760(EF_S32(0x001f71c8u) >> 16, EF_S32(object + 0x14u) >> 16);
    return result;
}

/* 0x7ca8, 120 bytes. Scaled point from a table entry and one sub-entry, stored as two doubles. */
void scaled_entry_point_7ca8(ef_addr32 out, uint8_t index, uint8_t sub)
{
    ef_addr32 entry = EF_U32(0x001972e0u) + (uint32_t)index * 34u;
    ef_addr32 data;
    double scale;

    EF_F64(out) = guest_fn_b4230(EF_S32(entry + 0xau) >> 16);
    EF_F64(out + 8u) = guest_fn_b4230(EF_S32(entry + 6u) >> 16);
    data = EF_U32(entry + 0x1eu) + (uint32_t)sub * 25u;
    scale = EF_F64(0x001803f0u);
    EF_F64(out) = (double)EF_S16(data) * scale + EF_F64(out);
    EF_F64(out + 8u) = (double)EF_S16(data + 2u) * scale + EF_F64(out + 8u);
}

/* 0x8184, 104 bytes. Index of the table entry with the smallest metric, or 0xff. */
uint8_t find_min_metric_index_8184(ef_addr32 context)
{
    double best;
    uint32_t bestIndex = 0xff;
    int32_t i;
    uint32_t raw[2];

    raw[0] = 0x78b58c40u;
    raw[1] = 0x4415af1du;
    best = *(double *)raw;
    for (i = 0; i < (EF_S32(0x001972e4u) >> 16); i++) {
        double value = guest_fn_80f4(context, (uint32_t)i);
        if (value < best) {
            best = value;
            bestIndex = (uint32_t)i;
        }
    }
    return (uint8_t)bestIndex;
}

/* 0x81ec, 111 bytes. Double value of a sub-entry, wrapped through a helper unless its kind is 1. */
double sub_entry_value_81ec(uint8_t index, uint8_t sub)
{
    ef_addr32 entry = EF_U32(0x001972e0u) + (uint32_t)index * 34u;
    ef_addr32 rec = EF_U32(entry + 0x16u) + (uint32_t)sub * 56u;

    if ((EF_S32(rec + 0x21u) >> 24) == 1)
        return EF_F64(rec + 0x18u);
    return guest_fn_b42f0(EF_F64(rec + 0x18u) + EF_F64(0x00180408u));
}

/* 0x1d844, 100 bytes. Pass the scaled offset of the current waypoint to a helper. */
void report_waypoint_offset_1d844(ef_addr32 context)
{
    ef_addr32 entry = EF_U32(context + 0xa1u) + (uint32_t)EF_U8(context + 0xa5u) * 25u;
    double scale = EF_F64(0x00181960u);
    double a = (double)EF_S32(entry + 4u) * scale - EF_F64(context + 0x51u);
    double b = (double)EF_S32(entry) * scale - EF_F64(context + 0x59u);

    guest_fn_b4620(b, a);
}

/* 0x1d8e8, 106 bytes. True when the current waypoint is within the limit on either axis. */
uint8_t waypoint_near_1d8e8(ef_addr32 context)
{
    ef_addr32 entry = EF_U32(context + 0xa1u) + (uint32_t)EF_U8(context + 0xa5u) * 25u;
    double scale = EF_F64(0x00181968u);
    double x = (double)EF_S32(entry + 4u) * scale - EF_F64(context + 0x51u);
    double y = (double)EF_S32(entry) * scale - EF_F64(context + 0x59u);
    double limit = EF_F64(0x00181970u);

    if (x < 0)
        x = -x;
    if (x < limit)
        return 1;
    if (y < 0)
        y = -y;
    return !(y >= limit);
}

/* 0x29adc, 55 bytes. Two scaled components from a record's rotation words. */
void scaled_rotation_29adc(ef_addr32 record, ef_addr32 out1, ef_addr32 out2)
{
    double k1 = EF_F64(0x00182494u);
    double k2 = EF_F64(0x0018249cu);
    int32_t v = -(EF_S32(record + 0xau) >> 16);

    EF_F64(out1) = (double)v * k1 * k2;
    EF_F64(out2) = (double)EF_S16(record + 0xeu) * k1 * k2;
}

/* 0x2aa08, 42 bytes. Convert a packed 16.16 pair to doubles and clear the third component. */
void unpack_pair_2aa08(ef_addr32 out, ef_addr32 in)
{
    EF_F64(out) = guest_fn_b4230(EF_S16(in));
    EF_F64(out + 8u) = guest_fn_b4230(EF_S32(in) >> 16);
    EF_U32(out + 0x10u) = 0;
    EF_U32(out + 0x14u) = 0;
}

/* 0x2dc84, 90 bytes. True when a record is close enough and active. */
uint8_t record_close_and_active_2dc84(int16_t index)
{
    uint8_t result = 0;
    ef_addr32 base;

    if (index == 0xff)
        return 0;
    base = EF_U32(0x0019bb58u) + (uint32_t)(int32_t)index * 0xcau;
    if (guest_fn_b415c(0x001a3020u, base + 0x61u) < EF_F64(0x001252a4u))
        result = 1;
    if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(int32_t)index * 0xcau + 0x1au) == 0)
        result = 0;
    return result;
}

/* 0x37bc8, 66 bytes. Distance helper over a scaled pair taken from a table entry. */
double scaled_pair_distance_37bc8(ef_addr32 out, int16_t index)
{
    double pair[2];
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833a8u);

    pair[0] = (double)EF_S32(entry + 0xfu) * scale;
    pair[1] = (double)EF_S32(entry + 7u) * scale;
    return guest_fn_b415c(out, (ef_addr32)(uintptr_t)pair);
}
