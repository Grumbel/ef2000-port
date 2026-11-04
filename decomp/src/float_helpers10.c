/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <math.h>

/* Tenth x87 batch: truncation, direction vectors and 3x3 double matrix products. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x9d72c, 29 bytes. Truncate ST0 toward zero (FPU control word temporarily set to round-to-zero). */
double truncate_9d72c(double value)
{
    return value < 0.0 ? ceil(value) : floor(value);
}

/* 0x6ebbc, 36 bytes. Store the wrapped difference of two angles scaled by a constant. */
void store_scaled_angle_difference_6ebbc(void)
{
    EF_F64(0x001f6f60u) = wrap_angle_b42f0(EF_F64(0x001f70f8u) - EF_F64(0x001f7130u)) * EF_F64(0x001866f4u);
}

/* 0x7b040, 76 bytes. Scale a 2D double point, look up the cell value and return it negated and scaled. */
double lookup_scaled_cell_7b040(ef_addr32 point)
{
    double k = EF_F64(0x00187c50u);
    int32_t iy = (int32_t)truncate_9d72c(EF_F64(point + 8u) * k);
    int32_t ix = (int32_t)truncate_9d72c(k * EF_F64(point));
    int32_t r = (int32_t)guest_fn_7b16c_x(0, (uint32_t)iy, (uint32_t)ix);

    return (double)(-r) * EF_F64(0x00187c58u);
}

/* 0x88fb4, 67 bytes. Unit direction vector from a heading (a) and pitch (b): (cos a cos b, sin a cos b, -sin b). */
void direction_vector_88fb4(ef_addr32 out, uint32_t u1, uint32_t u2, uint32_t u3, double a, double b)
{
    (void)u1; (void)u2; (void)u3;
    EF_F64(out) = cos(a) * cos(b);
    EF_F64(out + 8u) = cos(b) * sin(a);
    EF_F64(out + 16u) = -sin(b);
}

/* 0xb4508, 71 bytes. out = transpose(mat) * vec for a 3x3 double matrix stored by rows. */
void matrix_transpose_mul_vector_b4508(ef_addr32 out, ef_addr32 vec, ef_addr32 mat)
{
    uint32_t i;

    for (i = 0; i < 3; i++) {
        EF_F64(out + i * 8u) = EF_F64(mat + (3u + i) * 8u) * EF_F64(vec + 8u) + EF_F64(mat + i * 8u) * EF_F64(vec) +
                               EF_F64(mat + (6u + i) * 8u) * EF_F64(vec + 16u);
    }
}

/* 0xb4550, 71 bytes. out = mat * vec for a 3x3 double matrix stored by rows. */
void matrix_mul_vector_b4550(ef_addr32 out, ef_addr32 vec, ef_addr32 mat)
{
    uint32_t i;

    for (i = 0; i < 3; i++) {
        EF_F64(out + i * 8u) = EF_F64(mat + (3u * i + 1u) * 8u) * EF_F64(vec + 8u) + EF_F64(mat + 3u * i * 8u) * EF_F64(vec) +
                               EF_F64(mat + (3u * i + 2u) * 8u) * EF_F64(vec + 16u);
    }
}
