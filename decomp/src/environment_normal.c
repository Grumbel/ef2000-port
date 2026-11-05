/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xdbdd0, 88 bytes. Skips spaces, writes the advanced 0x425af0 cursor,
 * then reports nonempty, non-comment lines using a stack-local sprintf
 * buffer and the indirect guest error sink [0x132a74]. This host hook
 * represents the two unresolved guest calls and the stack buffer; it does
 * not assert that their argument ABI or format text has been recovered. */
void env_normal_dbdd0(void)
{
    ef_addr32 cursor = EF_U32(0x425af0u);
    while (EF_U8(cursor) == 0x20u)
        ++cursor;
    EF_U32(0x425af0u) = cursor;
    if (EF_U8(cursor) != (uint8_t)';' &&
        EF_U8(cursor) != (uint8_t)'\n' && EF_U8(cursor) != 0u)
        environment_report_normal_line_088(cursor, 0x190448u,
                                           EF_U32(0x132a74u));
}
