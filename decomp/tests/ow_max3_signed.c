/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

int32_t max3_signed(int32_t a, int32_t b, int32_t c)
{
    int32_t m = a > b ? a : b;
    return m > c ? m : c;
}
