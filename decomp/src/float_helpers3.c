/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Third x87 batch: unit table distances, vector differences and state setup. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x80f4, 144 bytes. Distance from a context to a table entry's centre, or a large sentinel. */
double entry_distance_80f4(ef_addr32 context, uint32_t index)
{
    double v[3];
    ef_addr32 entry;
    double scale;
    uint32_t raw[2];

    if ((int32_t)index >= (EF_S32(0x001972e4u) >> 16)) {
        raw[0] = 0x78b58c40u;
        raw[1] = 0x4415af1du;
        return *(double *)raw;
    }
    entry = EF_U32(0x001972e0u) + index * 34u;
    scale = EF_F64(0x00180400u);
    v[0] = (double)(int32_t)((EF_U32(entry + 0xcu) << 11) + 0x400u) * scale;
    v[1] = (double)(int32_t)((EF_U32(entry + 8u) << 11) + 0x400u) * scale;
    v[2] = 0.0;
    return guest_fn_b415c(context, (ef_addr32)(uintptr_t)v);
}

/* 0x8394, 144 bytes. Flag a sub-entry as ahead (1) or behind (0xff) from a direction test. */
void flag_sub_entry_side_8394(uint8_t index, uint8_t sub)
{
    ef_addr32 entry = EF_U32(0x001972e0u) + (uint32_t)index * 34u;
    ef_addr32 rec = EF_U32(entry + 0x16u) + (uint32_t)sub * 56u;
    double d = guest_fn_b42f0(guest_fn_b4620(EF_F64(0x001a3120u), EF_F64(0x001a3118u)) - EF_F64(rec + 0x18u));

    if (d >= EF_F64(0x00180420u))
        EF_U8(EF_U32(EF_U32(0x001972e0u) + (uint32_t)index * 34u + 0x16u) + (uint32_t)sub * 56u + 0x24u) = 0xff;
    else
        EF_U8(EF_U32(EF_U32(0x001972e0u) + (uint32_t)index * 34u + 0x16u) + (uint32_t)sub * 56u + 0x24u) = 1;
}

/* 0x8424, 145 bytes. Scaled heading of a sub-entry as an integer angle, wrapped positive. */
int32_t sub_entry_heading_8424(uint8_t index, uint8_t sub)
{
    ef_addr32 entry = EF_U32(0x001972e0u) + (uint32_t)index * 34u;
    ef_addr32 rec = EF_U32(entry + 0x16u) + (uint32_t)sub * 56u;
    double value = EF_F64(rec + 0x18u) * EF_F64(0x00180428u);

    if ((EF_S32(rec + 0x21u) >> 24) == -1)
        value = value + EF_F64(0x00180430u);
    if (0.0 > value)
        value = value + EF_F64(0x00180438u);
    return (int32_t)value;
}

/* 0x18118, 154 bytes. Toggle the autopilot hint flag when the player is in the right state. */
void toggle_hint_flag_18118(void)
{
    ef_addr32 unit = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);

    if (unit == 0 || EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0
        || EF_U16(0x00126516u) != 0 || EF_U8(0x00127d43u) != 0 || EF_U8(0x001a3634u) != 0)
        return;
    if (EF_U8(unit + 0x197u) > 0x32 || EF_U8(unit + 0x195u) >= 0x64)
        return;
    if (EF_F64(unit + 0x91u) > EF_F64(0x00181819u) && EF_U8(0x00126528u) == 0)
        return;
    EF_U8(0x00126528u) = (uint8_t)(1 - EF_U8(0x00126528u));
}

/* 0x1d7a4, 159 bytes. Initialise an autopilot context from its waypoint table. */
void init_autopilot_context_1d7a4(ef_addr32 context)
{
    ef_addr32 table = EF_U32(context + 0xa1u);
    double scale = EF_F64(0x00181958u);

    EF_F64(context + 0x51u) = (double)EF_S32(table + 4u) * scale;
    table = EF_U32(context + 0xa1u);
    EF_U32(context + 0x61u) = 0;
    EF_U32(context + 0x65u) = 0xbff00000u;
    EF_U32(context + 0x91u) = 0;
    EF_U32(context + 0x95u) = 0x40490000u;
    EF_U32(context + 0x79u) = 0;
    EF_U32(context + 0x7du) = 0x4075e000u;
    EF_U32(context + 0x69u) = 0x47ae147bu;
    EF_U32(context + 0x6du) = 0x3fb47ae1u;
    EF_F64(context + 0x59u) = (double)EF_S32(table) * scale;
    EF_U32(context + 0x99u) = 0;
    EF_U32(context + 0x9du) = 0x3ff00000u;
    EF_F64(context + 0x81u) = report_waypoint_offset_1d844(context);
    EF_U32(context + 0x89u) = 0;
    EF_U32(0x0019af78u) = context;
    EF_U32(context + 0x8du) = 0;
}

/* 0x6a5ac, 66 bytes. Helper result for the offset from a position to a unit, or 0.0. */
double unit_offset_value_6a5ac(ef_addr32 position, int16_t unit)
{
    ef_addr32 u = guest_fn_7489c(unit);

    if (u == 0)
        return 0.0;
    return guest_fn_b4620(EF_F64(u + 0x51u) - EF_F64(position + 8u), EF_F64(u + 0x49u) - EF_F64(position));
}

/* 0x6a66c, 121 bytes. Distance between two units, or 0.0 when either is missing. */
double unit_distance_6a66c(int16_t first, int16_t second)
{
    double v[3];
    double w[3];
    ef_addr32 a = guest_fn_6a1b4(first);
    ef_addr32 b = guest_fn_6a1b4(second);

    if (a == 0 || b == 0)
        return 0.0;
    v[0] = EF_F64(a + 0x49u) - EF_F64(b + 0x49u);
    v[1] = EF_F64(a + 0x51u) - EF_F64(b + 0x51u);
    v[2] = EF_F64(a + 0x59u) - EF_F64(b + 0x59u);
    w[0] = 0.0;
    w[1] = 0.0;
    w[2] = 0.0;
    return guest_fn_b41bc((ef_addr32)(uintptr_t)v, (ef_addr32)(uintptr_t)w);
}

/* 0x6a6e8, 117 bytes. Difference of two units' weighted axes, stored as three doubles. */
void unit_axis_difference_6a6e8(ef_addr32 out, int16_t first, int16_t second)
{
    ef_addr32 a = guest_fn_6a1b4(first);
    ef_addr32 b = guest_fn_6a1b4(second);

    if (a == 0 || b == 0)
        return;
    EF_F64(out) = EF_F64(b + 0x91u) * EF_F64(b + 0xa1u) - EF_F64(a + 0x91u) * EF_F64(a + 0xa1u);
    EF_F64(out + 8u) = EF_F64(b + 0x91u) * EF_F64(b + 0xb9u) - EF_F64(a + 0x91u) * EF_F64(a + 0xb9u);
    EF_F64(out + 0x10u) = EF_F64(b + 0x91u) * EF_F64(b + 0xd1u) - EF_F64(a + 0x91u) * EF_F64(a + 0xd1u);
}

/* 0x6bd88, 157 bytes. Copy the camera position and recompute the closing-speed state. */
void update_camera_state_6bd88(void)
{
    ef_addr32 unit = EF_U32(0x001f71bcu);

    EF_U32(0x001f7130u) = EF_U32(unit + 0x61u);
    EF_U32(0x001f7134u) = EF_U32(unit + 0x65u);
    if (1.0 < EF_F64(0x001f71b0u)) {
        EF_F64(0x001f7138u) = (EF_F64(0x001f6f88u) + EF_F64(0x001f7068u)) * EF_F64(0x001863fcu) / EF_F64(0x001f71b0u);
    } else {
        EF_U32(0x001f7138u) = 0;
        EF_U32(0x001f713cu) = 0;
    }
    EF_U32(0x001f7050u) = 0;
    EF_U32(0x001f7054u) = 0x3ff00000u;
    EF_F64(0x001f70c8u) = -EF_F64(0x001f7138u) * EF_F64(0x00186404u);
    guest_fn_6c2a4();
    if (EF_U8(EF_U32(0x001f71bcu) + 0x18du) == 0)
        guest_fn_6bee8();
    guest_fn_6be28();
}

/* 0x84f60, 144 bytes. Double stored in a unit's slot table, or 0.0 when the unit is missing. */
double unit_slot_value_84f60(int16_t unit, uint8_t slot)
{
    ef_addr32 u = guest_fn_7489c(unit);
    ef_addr32 base;
    ef_addr32 p;

    if (u == 0)
        return 0.0;
    base = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(u + 0x26u) >> 16) * 0xcau;
    if (EF_U32(base + 0x8au) == 0)
        guest_report_parser_enum_error(0x00189c1cu, (uint32_t)(int32_t)unit, (uint32_t)(EF_S32(u + 0x26u) >> 16));
    base = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(u + 0x26u) >> 16) * 0xcau;
    p = EF_U32(base + 0x8au) + (uint32_t)slot * 25u;
    return EF_F64(p + 8u);
}

/* 0x86720, 146 bytes. Sum the weighted entries of the player's slot list into two doubles. */
void sum_slot_weights_86720(void)
{
    ef_addr32 unit = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    uint8_t i;

    if (unit == 0)
        return;
    EF_U32(0x001a30b0u) = 0;
    EF_U32(0x001a30b4u) = 0;
    EF_U32(0x001a3018u) = 0;
    EF_U32(0x001a301cu) = 0;
    for (i = 0; i < 0x14; i++) {
        ef_addr32 slot = unit + (uint32_t)i * 3u;
        uint32_t row = (uint32_t)(EF_S32(slot + 0x198u) >> 24) * 0x91u;
        double w = (double)EF_S16(slot + 0x19cu);

        EF_F64(0x001a30b0u) = w * EF_F64(row + 0x0012bc99u) + EF_F64(0x001a30b0u);
        EF_F64(0x001a3018u) = (double)(w * (double)*(float *)ef_guest_memory_ptr(row + 0x0012bca8u))
                              + EF_F64(0x001a3018u);
    }
}

/* 0x88d90, 74 bytes. Scale a three-double vector in place and store its truncated components. */
void scale_vector_to_ints_88d90(ef_addr32 out, ef_addr32 vec)
{
    double k1 = EF_F64(0x0018a4a0u);
    double k2 = EF_F64(0x0018a4a8u);

    EF_F64(vec) = EF_F64(vec) * k1 * k2;
    EF_F64(vec + 8u) = EF_F64(vec + 8u) * k1 * k2;
    EF_F64(vec + 0x10u) = EF_F64(vec + 0x10u) * k1 * k2;
    EF_S32(out) = (int32_t)EF_F64(vec + 8u);
    EF_S32(out + 4u) = (int32_t)EF_F64(vec + 0x10u);
    EF_S32(out + 8u) = (int32_t)EF_F64(vec);
}
