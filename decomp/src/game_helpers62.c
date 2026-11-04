/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* Code address aliasing. */

/* 0xfd404, 276 bytes. Redirect any of a fixed set of code addresses to the shared handler 0xfd52f. */
uint32_t alias_code_address_fd404(uint32_t address)
{
    static const uint32_t aliases[] = { 0xfd59f,0xfd5bd,0xfd619,0xfd637,0xfd695,0xfd6b2,0xfd731,0xfd7b0,0xfd7ce,0xfd82a,0xfd848,0xfd8a6,0xfd8c4,0xfd926,0xfd943,0xfd9e9,0xfda07,0xfda64,0xfda82,0xfdadf,0xfdafc,0xfdb5d,0xfdb7b,0xfdbf8,0xfdc16,0xfdc73,0xfdc91,0xfdcee,0xfdd0c,0xfdd69,0xfdd86 };
    uint32_t i;

    for (i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (address == aliases[i])
            return 0x000fd52fu;
    }
    return address;
}
