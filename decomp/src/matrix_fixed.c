/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 3x3 fixed-point (24.8) matrix products. The originals take ESI, EBX and EDI as register parameters. */

static int32_t dot3_fixed(int32_t a0, int32_t b0, int32_t a1, int32_t b1, int32_t a2, int32_t b2)
{
    int64_t sum = (int64_t)a0 * b0 + (int64_t)a1 * b1 + (int64_t)a2 * b2;

    return (int32_t)(sum >> 8);
}

/* 0xfceb4, 327 bytes. out = a * transpose(b); inputs in ESI (a), EBX (b), output EDI. */
void matrix_mul_transposed_fceb4(ef_addr32 a, ef_addr32 b, ef_addr32 out)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            EF_S32(out + (i * 3u + j) * 4u) = dot3_fixed(
                EF_S32(a + i * 12u), EF_S32(b + j * 12u),
                EF_S32(a + i * 12u + 4u), EF_S32(b + j * 12u + 4u),
                EF_S32(a + i * 12u + 8u), EF_S32(b + j * 12u + 8u));
        }
    }
}

/* 0xfcffb, 327 bytes. out = a * b; inputs in ESI (a), EBX (b), output EDI. */
void matrix_mul_fcffb(ef_addr32 a, ef_addr32 b, ef_addr32 out)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            EF_S32(out + (i * 3u + j) * 4u) = dot3_fixed(
                EF_S32(a + i * 12u), EF_S32(b + j * 4u),
                EF_S32(a + i * 12u + 4u), EF_S32(b + 12u + j * 4u),
                EF_S32(a + i * 12u + 8u), EF_S32(b + 24u + j * 4u));
        }
    }
}
