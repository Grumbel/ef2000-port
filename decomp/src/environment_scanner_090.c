/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_090.h"
#include "ef_mem.h"

/* A repeated original inline loop, NOT a call to an invented original
 * function. Every callback scans until LF or NUL, replaces the delimiter
 * with NUL, advances the shared cursor and clears the preceding byte.
 * On empty/unterminated lines the same potentially surprising writes occur
 * in the original binary. Some callbacks clear [end-1] before incrementing
 * the cursor, others clear [end-2] afterward; both address the same byte. */
static ef_addr32 take_line(void)
{
    ef_addr32 start = EF_U32(0x425af8u);
    ef_addr32 end = start;
    while (EF_U8(end) != 0u && EF_U8(end) != (uint8_t)'\n')
        ++end;
    EF_U8(end) = 0u;
    EF_U8(end - 1u) = 0u;
    EF_U32(0x425af8u) = end + 1u;
    return start;
}

void env_scan_line_090(uint32_t entry, uint32_t format,
                       ef_environment_fields_090 *fields)
{
    ef_addr32 line = take_line();
    env_scan_fields_090(entry, format, line, fields);
}

void env_report_090(uint32_t message_address)
{
    (void)env_engine_call_090(0x132a74u, message_address, 0u, 0u);
}
