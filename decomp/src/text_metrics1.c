/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Formatted text width. */

static uint32_t formatted_width_common(ef_addr32 fmt, va_list ap, uint32_t shift)
{
    char buf[0x50];
    uint32_t w;
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    if (EF_U8(0x0012b369u) == 0u)
        return text_width_159a0((uint32_t)(uintptr_t)buf);
    w = guest_fn_b1b8b((uint32_t)(uintptr_t)buf);
    return w << shift;
}

/* 0x48d5c, 68 bytes. printf-format a string and return its width: the text width for zoom 0, else its length times 4. Variadic; all arguments on the stack. */
uint32_t formatted_width_48d5c(ef_addr32 fmt, ...)
{
    va_list ap;
    uint32_t w;
    va_start(ap, fmt);
    w = formatted_width_common(fmt, ap, 2u);
    va_end(ap);
    return w;
}

/* 0x48da0, 68 bytes. Same with length times 8 for the nonzero zoom. */
uint32_t formatted_width_48da0(ef_addr32 fmt, ...)
{
    va_list ap;
    uint32_t w;
    va_start(ap, fmt);
    w = formatted_width_common(fmt, ap, 3u);
    va_end(ap);
    return w;
}
