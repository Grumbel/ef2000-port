/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Eleventh x87 batch: global matrix transform and tick delta. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_F32(a) (*(float *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xea038, 99 bytes. out = M * vec with the 3x3 double matrix at 0x464480 (rows are 0x18 bytes apart). */
void transform_by_global_matrix_ea038(ef_addr32 out, ef_addr32 vec)
{
    uint32_t i;

    for (i = 0; i < 3; i++) {
        ef_addr32 row = 0x00464480u + i * 0x18u;

        EF_F64(out + i * 8u) = EF_F64(row + 8u) * EF_F64(vec + 8u) + EF_F64(row) * EF_F64(vec) +
                               EF_F64(row + 16u) * EF_F64(vec + 16u);
    }
}

/* 0xe3120, 57 bytes. Sample the tick counter and store the scaled change since the previous sample. */
void update_tick_rate_e3120(void)
{
    float now = (float)(double)guest_fn_f8248_x();
    float delta = (float)((double)now - (double)EF_F32(0x0015f8f0u));

    EF_F32(0x0015f8f0u) = now;
    EF_F32(0x0015f8f4u) = (float)((double)delta * (double)EF_F32(0x001908dcu));
}
