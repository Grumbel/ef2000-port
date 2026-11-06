/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"

void init_bounds_d0b68(ef_addr32 record)
{
    volatile uint32_t *bounds = (volatile uint32_t *)(uintptr_t)record;
    bounds[3] = 0;
    bounds[6] = 0;
    bounds[1] = 0;
    bounds[7] = 0;
    bounds[2] = 0;
    bounds[5] = 0;
    bounds[0] = 0x7fffu;
    bounds[4] = 0x7fffu;
    bounds[8] = 0x7fffu;
}
