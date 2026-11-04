/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Second x87 batch: proximity tests, angle conversions and sine tables. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x2dbf8, 137 bytes. True when a record is near the unit entry and active. */
uint8_t record_near_unit_2dbf8(int16_t index, uint8_t unit)
{
    uint8_t result = 0;
    uint32_t off = (uint32_t)unit * 27u;

    if (index == 0xff)
        return 0;
    if (guest_fn_7489c(EF_S32(0x001a5c2eu + off) >> 16) != 0) {
        if (guest_fn_b415c(EF_U32(0x0019bb58u) + (uint32_t)(int32_t)index * 0xcau + 0x61u,
                           0x001a5c30u + off + 3u) < EF_F64(0x001252a4u))
            result = 1;
    }
    if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(int32_t)index * 0xcau + 0x1au) == 0)
        result = 0;
    return result;
}

/* 0x37c0c, 86 bytes. Return the helper result for the offset between a table point and a position (ST0). */
double report_point_offset_37c0c(ef_addr32 position, int16_t index)
{
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833b0u);
    double a = (double)EF_S32(entry + 0xfu) * scale;
    double b = (double)EF_S32(entry + 7u) * scale;

    return guest_fn_b4620(b - EF_F64(position + 8u), a - EF_F64(position));
}

/* 0x37c64, 117 bytes. True when a table point is close to a unit's position. */
uint8_t point_near_unit_37c64(int16_t index, uint8_t unit)
{
    double v[3];
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833b8u);
    double d;

    v[0] = (double)EF_S32(entry + 0xfu) * scale;
    v[1] = (double)EF_S32(entry + 7u) * scale;
    v[2] = (double)EF_S32(entry + 0xbu) * scale;
    d = guest_fn_b415c((ef_addr32)(uintptr_t)v, 0x001a5c30u + (uint32_t)unit * 27u + 3u);
    return !(d >= EF_F64(0x001833c0u));
}

/* 0x37cdc, 96 bytes. True when a table point is close to the player. */
uint8_t point_near_player_37cdc(int16_t index)
{
    double v[3];
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833c8u);
    double d;

    v[0] = (double)EF_S32(entry + 0xfu) * scale;
    v[1] = (double)EF_S32(entry + 7u) * scale;
    v[2] = (double)EF_S32(entry + 0xbu) * scale;
    d = guest_fn_b415c(0x001a3020u, (ef_addr32)(uintptr_t)v);
    return !(d >= EF_F64(0x001833d0u));
}

/* 0x37d88, 117 bytes. True unless a table point is far from a unit's position. */
uint8_t point_not_far_from_unit_37d88(int16_t index, uint8_t unit)
{
    double v[3];
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833d8u);
    double d;

    v[0] = (double)EF_S32(entry + 0xfu) * scale;
    v[1] = (double)EF_S32(entry + 7u) * scale;
    v[2] = (double)EF_S32(entry + 0xbu) * scale;
    d = guest_fn_b415c((ef_addr32)(uintptr_t)v, 0x001a5c30u + (uint32_t)unit * 27u + 3u);
    return !(d >= EF_F64(0x001833e0u));
}

/* 0x37e00, 96 bytes. True unless a table point is far from the player. */
uint8_t point_not_far_from_player_37e00(int16_t index)
{
    double v[3];
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)index * 69u;
    double scale = EF_F64(0x001833e8u);
    double d;

    v[0] = (double)EF_S32(entry + 0xfu) * scale;
    v[1] = (double)EF_S32(entry + 7u) * scale;
    v[2] = (double)EF_S32(entry + 0xbu) * scale;
    d = guest_fn_b415c(0x001a3020u, (ef_addr32)(uintptr_t)v);
    return !(d >= EF_F64(0x001833f0u));
}

/* 0x40870, 77 bytes. Rate between two clock readings and a distance, minus a divided distance. */
double clock_rate_difference_40870(uint32_t a, ef_addr32 b, uint32_t c, double divisor)
{
    double base = (double)guest_fn_40774(0x0012583cu, c);
    double later = (double)guest_fn_40774(a, c);
    double x = later - base;

    return x - guest_fn_b415c(b, c) / divisor;
}

/* 0x45b60, 50 bytes. Double value of an object, reporting an error when its check fails. */
double object_double_value_45b60(ef_addr32 object)
{
    if (((uint32_t (*)(ef_addr32))EF_U32(0x00126650u))(object) == 0)
        guest_report_parser_enum_error(0x00184654u, EF_U32(0x00126634u + EF_U32(object) * 4u), EF_U32(object + 4u));
    return EF_F64(object + 0x18u);
}

/* 0x45fc4, 69 bytes. Scale a double to an angle word, wrapping negatives. */
int32_t scale_to_angle_45fc4(double value)
{
    value = value * EF_F64(0x001846b6u) * EF_F64(0x001846beu);
    if (0.0 > value)
        value = value + EF_F64(0x001846c6u);
    return (int32_t)value;
}

/* 0x4600c, 42 bytes. Negated scaled double truncated to a byte. */
uint8_t scale_to_byte_4600c(double value)
{
    value = -value * EF_F64(0x001846ceu) * EF_F64(0x001846d6u);
    return (uint8_t)(int32_t)value;
}

/* 0x4fdf4, 89 bytes. Add a float of minutes to a minutes/seconds pair, carrying seconds. */
void add_float_time_4fdf4(ef_addr32 minutes, ef_addr32 seconds, float amount)
{
    int32_t whole = (int32_t)amount;
    double frac = (double)amount - (double)(int16_t)whole;
    int32_t add = (int32_t)(frac * (double)*(float *)ef_guest_memory_ptr(0x00184eb4u));
    int16_t s = (int16_t)(EF_U16(seconds) + add);

    EF_U16(seconds) = (uint16_t)s;
    if (s > 0x3b) {
        EF_U16(minutes) = (uint16_t)(EF_U16(minutes) + 1);
        EF_U16(seconds) = (uint16_t)(EF_U16(seconds) - 0x3c);
    }
    EF_U16(minutes) = (uint16_t)(EF_U16(minutes) + (uint16_t)whole);
}

/* 0x53330, 32 bytes. Length of a vector of three floats. */
double float_vector_length_53330(float x, float y, float z)
{
    return guest_fn_a4470((double)x * (double)x + (double)y * (double)y + (double)z * (double)z);
}

/* 0x54110, 58 bytes. Store the scaled frame step and refresh the dependent tables. */
void refresh_frame_tables_54110(void)
{
    EF_F64(0x001a5c10u) = (double)EF_U16(0x00127d92u) * EF_F64(0x00185ba4u);
    guest_fn_5414c();
    guest_fn_54210();
    guest_fn_54274();
    guest_fn_54328();
    guest_fn_5437c();
}

/* 0x5f944, 88 bytes. Fill a 90-entry scaled sine table. */
void fill_sine_table_5f944(void)
{
    int16_t i;

    for (i = 0; i < 0x5a; i++) {
        double angle = (double)i * EF_F64(0x00186034u) * EF_F64(0x0018603cu);
        EF_U16(0x001a7c5cu + (uint32_t)(int32_t)i * 2u) = (uint16_t)(int32_t)(guest_fn_b45fa(angle) * EF_F64(0x00186044u));
    }
}
