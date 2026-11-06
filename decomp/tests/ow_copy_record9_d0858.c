/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"

void copy_record9_d0858(ef_addr32 destination, ef_addr32 source)
{
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)destination;
    volatile uint32_t *src = (volatile uint32_t *)(uintptr_t)source;
    uint32_t row, column;

    for (row = 0; row < 3u; ++row)
        for (column = 0; column < 3u; ++column)
            dst[column * 3u + row] = src[column * 3u + row];
}
