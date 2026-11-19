/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xdbdd0, 88 bytes. Advance the parser cursor past spaces and report
 * nonempty, non-comment lines through the original cdecl error callback. */
void env_normal_dbdd0(void)
{
    ef_addr32 cursor = EF_U32(0x425af0u);
    uint8_t first;
    char message[100];

    while ((first = EF_U8(cursor)) == ' ')
        ++cursor;
    EF_U32(0x425af0u) = cursor;
    if (first != ';' && first != '\n' && first != 0u) {
        ef_addr32 buffer = (ef_addr32)(uintptr_t)message;
        guest_sprintf_a4342(buffer, 0x190448u, cursor);
        guest_callptr1_x(EF_U32(0x132a74u), buffer);
    }
}
