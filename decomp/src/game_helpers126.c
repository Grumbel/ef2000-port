/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include "ef_abi.h"
#include "ef_globals.h"

/* Formatted write to a handle. */

/* 0xb1bb0, 70 bytes. printf to a handle (100..105): format into a 0x7c-byte buffer and write it (cdecl, variadic; all arguments on the stack). */
void print_to_handle_b1bb0(uint32_t handle, ef_addr32 fmt, ...)
{
    char buf[0x7c];
    va_list ap;
    uint32_t len;

    va_start(ap, fmt);
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    len = guest_fn_b1b8b((uint32_t)(uintptr_t)buf);
    write_handle_a7a1c(0, 0, 0, 0, handle, len, (uint32_t)(uintptr_t)buf);
    va_end(ap);
}
