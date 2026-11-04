/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <math.h>

/* Ninth x87 batch: scaled trig wrappers. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xf8278, 29 bytes. sin of the argument scaled by the two constants used in 0xf8250 (0x1932a4 and 0x1932ac). */
double sin_scaled_f8278(double angle)
{
    return sin(angle * EF_F64(0x001932a4u) * EF_F64(0x001932acu));
}

/* 0xf8298, 29 bytes. cos of the same scaled argument. */
double cos_scaled_f8298(double angle)
{
    return cos(angle * EF_F64(0x001932a4u) * EF_F64(0x001932acu));
}

/* 0xb45fa, 21 bytes. tan of ST0 (fptan, popping the pushed 1.0); with bit 0 of 0x1631f0 set the library emulation 0xfa3d4 is used instead. */
double tan_b45fa(double angle)
{
    return tan(angle);
}

/* 0xdc1d4, 21 bytes. atan of ST0 (fld1; fpatan); the emulation path 0x10848c is used when bit 0 of 0x1631f0 is set. */
double atan_dc1d4(double value)
{
    return atan(value);
}

/* 0x11ac3c, 35 bytes. Fetch a double from the table at [obj+0xf]; the index is built from three bytes with the multipliers at obj+1 and obj+2 (8-bit arithmetic): (a * [obj+1] * [obj+2] + b * [obj+2] + c) & 0xff. */
double table_double_11ac3c(uint32_t obj, uint32_t a, uint32_t b, uint32_t c)
{
    uint8_t m2 = EF_U8(obj + 2u);
    uint8_t t = (uint8_t)(((uint8_t)((a & 0xffu) * EF_U8(obj + 1u))) * m2);
    uint8_t u = (uint8_t)((b & 0xffu) * m2);
    uint8_t idx = (uint8_t)(t + u + (c & 0xffu));
    return EF_F64(EF_U32(obj + 0xfu) + (uint32_t)idx * 8u);
}

/* 0x10d9c6, 25 bytes. (ret 12) modf: truncate x toward zero (0x9d72c), store the integral part through ip and return the fractional part. */
double modf_10d9c6(double x, uint32_t ip)
{
    double whole = trunc(x);
    EF_F64(ip) = whole;
    return x - whole;
}

/* 0x11b588, 30 bytes. (ret 8) Call 0x11b540 with the double argument and scale its result by the constant at 0x1958bc. */
double scaled_call_11b588(double x)
{
    return guest_fn_11b540_x(x) * EF_F64(0x001958bcu);
}

/* 0x11bf00, 37 bytes. (ret 16) Linear combination: f(a0) * v[0] + v[1] * a1 with f computed by 0x11bf00's helper 0xb3fd0 and the pair of doubles at EAX. */
double combine_11bf00(uint32_t v, double a0, double a1)
{
    return guest_fn_b3fd0_x(a0) * EF_F64(v) + EF_F64(v + 8u) * a1;
}

/* 0xdc1e9, 21 bytes. fpatan after an exchange: atan2(st0, st1) of the two x87 registers (ST0 = y, ST1 = x on entry); the emulation path 0x10848c is used when bit 0 of 0x1631f0 is set. */
double atan2_dc1e9(double st0, double st1)
{
    return atan2(st0, st1);
}

/* 0x11af60, 39 bytes. |((p[3] - p[2]) + (p[7] - p[6])) * ((p[4] - p[0]) * k)| for the doubles at EAX, with k at 0x195804. */
double abs_area_11af60(uint32_t p)
{
    double sum = (EF_F64(p + 0x18u) - EF_F64(p + 0x10u)) + (EF_F64(p + 0x38u) - EF_F64(p + 0x30u));
    double e = (EF_F64(p + 0x20u) - EF_F64(p)) * EF_F64(0x00195804u);
    return abs_double_b427c(sum * e);
}

/* 0x11b0ec, 40 bytes. atan2(p[1] - p[5], |p[4] - p[0]|) for the doubles at EAX. */
double slope_angle_11b0ec(uint32_t p)
{
    double dy = EF_F64(p + 8u) - EF_F64(p + 0x28u);
    double dx = abs_double_b427c(EF_F64(p + 0x20u) - EF_F64(p));
    return atan2_dc1e9(dy, dx);
}

/* 0xe86f7, 60 bytes. (ret 8) Floor-like: take the integral part with 0x10d9c6 and add the constant at 0x190c8c when the fractional part is negative. */
double floor_e86f7(double x)
{
    double whole;
    double frac = modf_10d9c6(x, (uint32_t)(uintptr_t)&whole);
    if (frac < 0.0)
        whole += EF_F64(0x00190c8cu);
    return whole;
}

/* 0x10a030, 72 bytes. Build a record at EAX from the one at EDX: zero the first 8 bytes, store the extent F64[+0x20] - F64[+0], and copy the dword pairs at +0x10..0x1f and +0x30..0x3f. */
void build_extent_record_10a030(uint32_t dst, uint32_t src)
{
    EF_U32(dst) = 0u;
    EF_U32(dst + 4u) = 0u;
    EF_F64(dst + 0x20u) = EF_F64(src + 0x20u) - EF_F64(src);
    EF_U32(dst + 0x10u) = EF_U32(src + 0x10u);
    EF_U32(dst + 0x14u) = EF_U32(src + 0x14u);
    EF_U32(dst + 0x18u) = EF_U32(src + 0x18u);
    EF_U32(dst + 0x1cu) = EF_U32(src + 0x1cu);
    EF_U32(dst + 0x30u) = EF_U32(src + 0x30u);
    EF_U32(dst + 0x34u) = EF_U32(src + 0x34u);
    EF_U32(dst + 0x38u) = EF_U32(src + 0x38u);
    EF_U32(dst + 0x3cu) = EF_U32(src + 0x3cu);
}

/* 0xea8cc, 66 bytes. (ret 4) Scale the three signed 32-bit components at EAX by the float whose bits are passed on the stack, truncating each product toward zero (0x9d72c). */
void scale_vector3_ea8cc(uint32_t vec, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t scale_bits)
{
    float s;
    uint32_t i;
    (void)u1; (void)u2; (void)u3;
    s = *(float *)&scale_bits;
    for (i = 0u; i < 3u; i++)
        EF_S32(vec + i * 4u) = (int32_t)trunc((double)EF_S32(vec + i * 4u) * (double)s);
}

/* 0x11aff4, 74 bytes. (ret 8) Blend two pairs of doubles by the fraction d and return atan2(a - b, |p[4] - p[0]|): a = p[3] * (1 - d) + p[2] * d, b = p[7] * (1 - d) + p[6] * d. */
double blend_angle_11aff4(uint32_t p, double d)
{
    double a = EF_F64(p + 0x18u) * (1.0 - d) + EF_F64(p + 0x10u) * d;
    double b = EF_F64(p + 0x38u) * (1.0 - d) + EF_F64(p + 0x30u) * d;
    double dx = abs_double_b427c(EF_F64(p + 0x20u) - EF_F64(p));
    return atan2_dc1e9(a - b, dx);
}

/* 0x11bfc8, 74 bytes. (ret 16) (((p[0] + k1) * k2 * k3 + k4) * (k5 / f(a0))) * k6 with f = 0x11af14 and the constants at 0x1959a4..0x1959cc. */
double scaled_ratio_11bfc8(uint32_t p, double a0, double a1)
{
    double r = guest_fn_11af14_x(a0);
    double x = (EF_F64(p) + EF_F64(0x001959a4u)) * EF_F64(0x001959acu) * EF_F64(0x001959b4u) + EF_F64(0x001959bcu);
    (void)a1;
    return x * (EF_F64(0x001959c4u) / r) * EF_F64(0x001959ccu);
}

/* 0x11b540, 69 bytes. (ret 8) f(x * k4) * 0.53 with f = 0xb3fec; the intermediate x * k1 * k2 + k3 is computed and discarded. */
double scaled_call_11b540(double x)
{
    return guest_fn_b3fec_x(x * EF_F64(0x001958b4u)) * 0.53;
}
