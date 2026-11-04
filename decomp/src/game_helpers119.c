/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* Tiny helpers. */

/* 0xa4470, 2 bytes. Return the constant 3. */
uint8_t return_three_a4470(void)
{
    return 3;
}

/* 0x7aa2c, 15 bytes. Register the status description built for the string at 0x187c00. */
void register_status_description_7aa2c(void)
{
    register_string_3c6c0(build_status_description(0x00187c00u));
}

/* 0xd5c84, 22 bytes. Map an address inside the range 0xd3900..0xd5c84 to 0xc454a; any other value is returned unchanged. */
uint32_t remap_address_d5c84(int32_t addr)
{
    if (addr >= 0xd3900 && addr <= 0xd5c84) {
        return 0x000c454au;
    }
    return (uint32_t)addr;
}
