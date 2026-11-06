/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

int32_t times_100(int32_t value)
{
    return (int32_t)((uint32_t)value * 100u);
}
