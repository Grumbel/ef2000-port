/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table setup and small formatted helpers. */

/* 0xd1f88, 53 bytes. Store the four resource addresses 0xd3c50, 0xff498, 0xd4250 and 0xff81c at 0x15b87c..0x15b888. */
void set_resource_addresses_d1f88(void)
{
    EF_U32(0x0015b87cu) = 0x000d3c50u;
    EF_U32(0x0015b880u) = 0x000ff498u;
    EF_U32(0x0015b884u) = 0x000d4250u;
    EF_U32(0x0015b888u) = 0x000ff81cu;
}

/* 0x40e20, 47 bytes. Copy the 12-byte record at 0x184358 to 0x41f30c, set the pending name to 0x184364, clear the word at 0x41eeda and store 1 at 0x137992. */
void reset_name_record_40e20(void)
{
    EF_U32(0x0041f30cu) = EF_U32(0x00184358u);
    EF_U32(0x0041f310u) = EF_U32(0x0018435cu);
    EF_U32(0x0041f314u) = EF_U32(0x00184360u);
    set_pending_name_ac7a8(0x00184364u);
    clear_word_41eeda();
    EF_U16(0x00137992u) = 1;
}

/* 0xa4342, 48 bytes. sprintf: format into buf (callback 0xa432f) and terminate it (cdecl, variadic). */
int32_t guest_sprintf_a4342(ef_addr32 buf, ef_addr32 fmt, ...)
{
    va_list ap;
    uint32_t arg_cursor;
    uint32_t length;

    va_start(ap, fmt);
    arg_cursor = (uint32_t)(uintptr_t)&ap[0];
    length = guest_fn_ed133_f(buf, fmt, (uint32_t)(uintptr_t)&arg_cursor, 0x000a432fu);
    va_end(ap);
    EF_U8(buf + length) = 0;
    return (int32_t)length;
}

/* 0xd5fc8, 52 bytes. When the flag at 0x15b968 is set, trace-call 0x1033c8 with the pointer at 0x420a6c and the value, and remember the low byte at 0x420a71. */
void note_value_d5fc8(uint32_t value)
{
    if (EF_U8(0x0015b968u) != 0) {
        trace_call_1033c8(EF_U32(0x00420a6cu), value);
        EF_U8(0x00420a71u) = (uint8_t)value;
    }
}
