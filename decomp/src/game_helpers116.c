/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Distance approximation and equipment cost maxima. */

/* 0xb39ec, 235 bytes. Approximate 3D distance of three components (15/16 of the largest magnitude plus 3/8 of the other two). */
int32_t approx_distance3_b39ec(int32_t a, int32_t b, int32_t c)
{
    int32_t x = (int32_t)abs_cf94a(a);
    int32_t y = (int32_t)abs_cf94a(b);
    int32_t z = (int32_t)abs_cf94a(c);
    int32_t t;

    if (x < y) { t = x; x = y; y = t; }
    if (x < z) { t = x; x = z; z = t; }
    if (y < z) { t = y; y = z; z = t; }
    return ((y + z) * 3) / 8 + (x * 15) / 16;
}

/* 0xa4eec, 274 bytes. Compute the current and maximum load figures of a loadout record from its flag bytes (+0x3a/+0x3b) and raise the stored maxima at +0x4b and +0x49. */
void update_loadout_maxima_a4eec(ef_addr32 rec)
{
    uint8_t f = EF_U8(rec + 0x3au);
    uint8_t g = EF_U8(rec + 0x3bu);
    uint32_t a = ((f & 0x20u) || (f & 0x40u) || (f & 0x80u) || (f & 4u)) ? 1u : 0u;
    uint32_t b = ((f & 8u) || (g & 1u)) ? 1u : 0u;
    uint32_t c = (f & 4u) ? 1u : 0u;
    uint32_t t = a + b + c;
    uint16_t cur = (uint16_t)(EF_U16(rec + 0x5fu) + EF_U16(rec + 0x5du) + t * 15u);
    uint16_t peak;

    if (f & 1u) {
        peak = (uint16_t)(EF_U16(rec + 0x61u) + 0x61u);
    } else {
        uint32_t count = ((f & 0x20u) ? 1u : 0u) + ((f & 0x40u) ? 1u : 0u) + ((f & 0x80u) ? 1u : 0u) + ((f & 4u) ? 1u : 0u);
        uint32_t d = ((f & 8u) ? 2u : 0u) + (((g & 1u) || (f & 4u)) ? 1u : 0u);
        uint32_t m = (count & 0xffffu) >= (d & 0xffffu) ? (count & 0xffffu) : (d & 0xffffu);

        peak = (uint16_t)(m * 15u + EF_U16(rec + 0x61u) + EF_U16(rec + 0x63u));
    }
    if (cur > EF_U16(rec + 0x4bu))
        EF_U16(rec + 0x4bu) = cur;
    if (peak > EF_U16(rec + 0x49u))
        EF_U16(rec + 0x49u) = peak;
}
