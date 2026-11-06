/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
int32_t to_lower_ascii(int32_t c)
{
    if (c >= 0x41 && c <= 0x5a)
        c += 0x20;
    return c;
}
