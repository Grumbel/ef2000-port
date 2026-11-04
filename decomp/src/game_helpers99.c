/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 64-bit plane evaluations, case-insensitive compare and palette scaling. */

/* Load the 64-bit accumulator of a plane record (+0x18) from its constant term at +0xc. */
static int64_t plane_load(ef_addr32 plane)
{
    EF_U32(plane + 0x18u) = EF_U32(plane + 0xcu);
    EF_U32(plane + 0x1cu) = EF_U32(plane + 0x10u);
    return (int64_t)(((uint64_t)EF_U32(plane + 0x10u) << 32) | EF_U32(plane + 0xcu));
}

static void plane_store(ef_addr32 plane, int64_t acc)
{
    EF_U32(plane + 0x18u) = (uint32_t)acc;
    EF_U32(plane + 0x1cu) = (uint32_t)((uint64_t)acc >> 32);
}

/* 0xce550, 39 bytes. Accumulate constant + z-coefficient * c into the plane's 64-bit result; returns 1 when the result is negative. */
uint8_t plane_eval_z_ce550(ef_addr32 plane, uint32_t u1, uint32_t u2, int32_t c)
{
    int64_t acc = plane_load(plane);

    (void)u1; (void)u2;
    acc += (int64_t)EF_S32(plane + 8u) * c;
    plane_store(plane, acc);
    return (uint8_t)(EF_S32(plane + 0x1cu) < 0);
}

/* 0xce4dc, 64 bytes. Evaluate a*x + b*y + c*z + constant into the plane's 64-bit result; returns the high dword. */
uint32_t plane_eval_hi_ce4dc(ef_addr32 plane, int32_t x, int32_t y, int32_t z)
{
    int64_t acc = plane_load(plane);

    acc += (int64_t)EF_S32(plane) * x;
    acc += (int64_t)EF_S32(plane + 4u) * y;
    acc += (int64_t)EF_S32(plane + 8u) * z;
    plane_store(plane, acc);
    return EF_U32(plane + 0x1cu);
}

/* 0xce498, 68 bytes. Same evaluation as ce4dc; returns 1 when the result is negative. */
uint8_t plane_eval_sign_ce498(ef_addr32 plane, int32_t x, int32_t y, int32_t z)
{
    int64_t acc = plane_load(plane);

    acc += (int64_t)EF_S32(plane) * x;
    acc += (int64_t)EF_S32(plane + 4u) * y;
    acc += (int64_t)EF_S32(plane + 8u) * z;
    plane_store(plane, acc);
    return (uint8_t)(EF_S32(plane + 0x1cu) < 0);
}

/* 0x10901d, 83 bytes. Case-insensitive compare of up to n bytes (does not stop at NUL); returns the first difference of the lower-cased bytes. */
int32_t compare_nocase_10901d(ef_addr32 a, ef_addr32 b, uint32_t n)
{
    while (n != 0) {
        uint8_t ca = EF_U8(a);
        uint8_t cb = EF_U8(b);

        if (ca >= 0x41 && ca <= 0x5a)
            ca = (uint8_t)(ca + 0x20);
        if (cb >= 0x41 && cb <= 0x5a)
            cb = (uint8_t)(cb + 0x20);
        if (ca != cb)
            return (int32_t)ca - (int32_t)cb;
        a++;
        b++;
        n--;
    }
    return 0;
}

/* 0x11082c, 101 bytes. Scale 8-bit palette chunks down to 6 bits in place (count byte, then per chunk a skipped byte, a colour count and 3 bytes per colour). */
void palette_8_to_6bit_11082c(ef_addr32 data)
{
    ef_addr32 p = data + 2u;
    int32_t count = EF_U8(data);
    int16_t e = 0;

    if ((int16_t)count <= 0)
        return;
    do {
        int16_t n;
        int16_t k = 0;

        p++;
        n = (int16_t)EF_U8(p);
        p++;
        if (n == 0)
            n = 0x100;
        while ((int32_t)k < (int32_t)n * 3) {
            EF_U8(p) = (uint8_t)(EF_U8(p) >> 2);
            p++;
            k++;
        }
        e++;
    } while (e < (int16_t)count);
}
