/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rotation of a 3x3 1.15 fixed-point matrix by one angle, one axis per function. */

/* Rotate columns ia and ib of every row of the matrix by the angle (13-bit table index). */
static void rotate_columns(ef_addr32 m, uint32_t ia, uint32_t ib, uint16_t angle)
{
    uint32_t idx = (uint32_t)angle >> 3;
    int32_t x = EF_S16(0x00142e08u + idx * 2u);
    int32_t y = EF_S16(0x00141e08u + idx * 2u);
    uint32_t row;

    for (row = 0; row < 3; row++) {
        ef_addr32 pa = m + (row * 3u + ia) * 4u;
        ef_addr32 pb = m + (row * 3u + ib) * 4u;
        int32_t a = EF_S32(pa);
        int32_t b = EF_S32(pb);
        int32_t new_a = (a * x - b * y) >> 15;
        int32_t new_b = (a * y + b * x) >> 15;

        EF_S32(pb) = new_b;
        EF_S32(pa) = (int32_t)(int16_t)new_a;
    }
}

/* 0xd0ebc, 251 bytes. Rotate the matrix's first and third columns by an angle. */
void update_scene_angle_first(ef_addr32 record, uint16_t value)
{
    rotate_columns(record, 0, 2, value);
}

/* 0xd0dbc, 254 bytes. Rotate the matrix's second and third columns by an angle. */
void update_scene_angle_second(ef_addr32 record, uint16_t value)
{
    rotate_columns(record, 1, 2, value);
}

/* 0xd0fb8, 251 bytes. Rotate the matrix's first and second columns by an angle. */
void update_scene_angle_third(ef_addr32 record, uint16_t value)
{
    rotate_columns(record, 0, 1, value);
}
