/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

/* 0xddbc8, 14 bytes. Signed 'a'..'z' to uppercase. */
int32_t to_upper_ascii(int32_t c)
{
    if (c >= 0x61 && c <= 0x7a)
        c -= 0x20;
    return c;
}

/* 0xe7835, 14 bytes. Signed 'A'..'Z' to lowercase. */
int32_t to_lower_ascii(int32_t c)
{
    if (c >= 0x41 && c <= 0x5a)
        c += 0x20;
    return c;
}
