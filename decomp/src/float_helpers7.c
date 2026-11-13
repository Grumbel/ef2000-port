/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Seventh x87 batch: integer vector length and distance helpers. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xea884, 71 bytes. Length of an integer 3D vector as a float, 0.0 for a negative sum. */
float int_vector_length_ea884(ef_addr32 vec)
{
    int32_t sum = EF_S32(vec + 4u) * EF_S32(vec + 4u);

    sum += EF_S32(vec) * EF_S32(vec);
    sum += EF_S32(vec + 8u) * EF_S32(vec + 8u);
    if (sum < 0)
        return 0.0f;
    return (float)(uint16_t)guest_square_root_of(sum);
}

/* 0xf8314, 78 bytes. Divide a value by a word scaled with 0x1932cc, 0.0 when the divisor is zero. */
double divide_by_scaled_word_f8314(int16_t n, uint32_t u1, uint32_t u2, uint32_t u3, double value)
{
    double scale = (double)n * EF_F64(0x001932ccu);

    (void)u1; (void)u2; (void)u3;
    if (scale != 0.0)
        return value / scale;
    return 0.0;
}

/* 0xf83d0, 79 bytes. Distance between two integer 2D points, 0.0 when coincident. */
double int_distance_2d_f83d0(int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
    int32_t dy = y2 - y1;
    int32_t dx = x2 - x1;
    double sum = (double)(dx * dx + dy * dy);

    if (sum != 0.0)
        return guest_fn_a4470(sum);
    return 0.0;
}

/* 0x6a4c4, 115 bytes. out = (object matrix at +0xa1) * vec for the object of the given unit index; no store when the lookup fails. */
void mul_object_matrix_vector(ef_addr32 out, ef_addr32 vec, int16_t unit)
{
    ef_addr32 m = guest_fn_6a1b4((int32_t)unit);
    double *r;
    double *v;
    double *k;

    if (m == 0u) {
        return;
    }
    r = (double *)ef_guest_memory_ptr(out);
    v = (double *)ef_guest_memory_ptr(vec);
    k = (double *)ef_guest_memory_ptr(m + 0xa1u);
    r[0] = k[1] * v[1] + k[0] * v[0] + k[2] * v[2];
    r[1] = k[4] * v[1] + k[3] * v[0] + k[5] * v[2];
    r[2] = k[7] * v[1] + k[6] * v[0] + k[8] * v[2];
}

/* 0x6a538, 115 bytes. out = transpose(object matrix at +0xa1) * vec. */
void mul_object_matrix_transpose_vector(ef_addr32 vec, ef_addr32 out, int16_t unit)
{
    ef_addr32 m = guest_fn_6a1b4((int32_t)unit);
    double *r;
    double *v;
    double *k;

    if (m == 0u) {
        return;
    }
    r = (double *)ef_guest_memory_ptr(out);
    v = (double *)ef_guest_memory_ptr(vec);
    k = (double *)ef_guest_memory_ptr(m + 0xa1u);
    r[0] = k[3] * v[1] + k[0] * v[0] + k[6] * v[2];
    r[1] = k[4] * v[1] + k[1] * v[0] + k[7] * v[2];
    r[2] = k[5] * v[1] + k[2] * v[0] + k[8] * v[2];
}
