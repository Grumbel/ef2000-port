/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9d749, 31 bytes. Copy sequential bytes in pairs, stopping immediately
 * at either terminator. Return the original destination; EDX is scratch. */
ef_addr32 copy_string_9d749(ef_addr32 destination, ef_addr32 source)
{
    ef_addr32 original = destination;
    for (;;) {
        uint8_t c = EF_U8(source);
        EF_U8(destination) = c;
        if (c == 0u) break;
        c = EF_U8(source + 1u);
        source += 2u;
        EF_U8(destination + 1u) = c;
        destination += 2u;
        if (c == 0u) break;
    }
    return original;
}
