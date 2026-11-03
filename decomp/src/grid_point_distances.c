/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <math.h>

static ef_world_grid_cell *grid_row(int16_t cell)
{
    return (ef_world_grid_cell *)((uint8_t *)ef_guest_memory_ptr(g_world_grid_base)
                                  + (int32_t)cell * 0xca);
}

/* 0x0002bab0, size 0x30, five distinct callers incl. 0x6f48 and
 * 0x7030. The original passes two *differences* by value to 0xb4620,
 * whose zero path accepts either +0/-0. Its nonzero path is a runtime
 * hypotenuse routine. Exact x87 rounding is still unverified. */
double grid_distance_to_point(ef_addr32 point, int16_t cell)
{
    const double *p = (const double *)ef_guest_memory_ptr(point);
    const ef_world_grid_cell *row = grid_row(cell);
    double dx = (double)((long double)row->position_a - p[0]);
    double dy = (double)((long double)row->position_b - p[1]);
    return sqrt((double)((long double)dx * dx + (long double)dy * dy));
}

/* 0x0002bae0, size 0x5b, six distinct callers. Scale is applied to each
 * point and each grid component *before* computing their 2D distance.
 * A third scaled component is computed but not read by 0xb415c. */
double scaled_grid_distance_to_point(ef_addr32 point, int16_t cell)
{
    const double *p = (const double *)ef_guest_memory_ptr(point);
    const ef_world_grid_cell *row = grid_row(cell);
    double a[3];
    double b[3];
    unsigned i;
    const double grid_xyz[3] = {row->position_a, row->position_b, row->position_c};
    for (i = 0; i < 3; ++i) {
        a[i] = (double)((long double)p[i] * g_grid_distance_scale);
        b[i] = (double)((long double)grid_xyz[i] * g_grid_distance_scale);
    }
    return sqrt((double)((long double)(a[0] - b[0]) * (a[0] - b[0]) +
                         (long double)(a[1] - b[1]) * (a[1] - b[1])));
}
