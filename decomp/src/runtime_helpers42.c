/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Stream printf family, open wrapper and file size. */

/* 0xfa815, 133 bytes. vfprintf: format into a stream through 0xed133 with the putc callback at 0xfa804, temporarily making an unbuffered-flagged stream line-buffered, flushing it afterwards; returns the character count or -1 on a stream error. */
int32_t stream_vprintf_fa815(ef_addr32 stream, ef_addr32 fmt, ef_addr32 args)
{
    uint32_t saved = EF_U32(stream + 0xcu) & 0x30u;
    int32_t count;
    int32_t unbuffered = 0;

    EF_U8(stream + 0xcu) &= 0xcf;
    if (EF_U32(stream + 8u) == 0)
        alloc_stream_buffer_10f071(stream);
    if ((EF_U8(stream + 0xdu) & 4) != 0) {
        EF_U8(stream + 0xdu) &= 0xfa;
        EF_U8(stream + 0xdu) |= 1;
        unbuffered = 1;
    }
    count = (int32_t)guest_fn_ed133_f(stream, fmt, args, 0x000fa804u);
    if (unbuffered != 0) {
        EF_U8(stream + 0xdu) &= 0xfa;
        EF_U8(stream + 0xdu) |= 4;
        stream_flush_fa8a0(stream);
    }
    if ((EF_U8(stream + 0xcu) & 0x20) != 0)
        count = -1;
    EF_U32(stream + 0xcu) |= saved;
    return count;
}

/* 0xbc400, 33 bytes. fprintf (cdecl, variadic). */
int32_t stream_printf_bc400(ef_addr32 stream, ef_addr32 fmt, ...)
{
    va_list ap;
    uint32_t arg_cursor;
    int32_t result;

    va_start(ap, fmt);
    arg_cursor = (uint32_t)(uintptr_t)&ap[0];
    result = stream_vprintf_fa815(stream, fmt, (uint32_t)(uintptr_t)&arg_cursor);
    va_end(ap);
    return result;
}

/* 0xcd12e, 34 bytes. printf to the standard output stream at 0x16043e (cdecl, variadic). */
int32_t stdout_printf_cd12e(ef_addr32 fmt, ...)
{
    va_list ap;
    uint32_t arg_cursor;
    int32_t result;

    va_start(ap, fmt);
    arg_cursor = (uint32_t)(uintptr_t)&ap[0];
    result = stream_vprintf_fa815(0x0016043eu, fmt, (uint32_t)(uintptr_t)&arg_cursor);
    va_end(ap);
    return result;
}

/* 0xfa485, 36 bytes. open: forward name, flags and the optional permission argument to 0xfa4a9 with share mode 0 (cdecl, variadic). */
int32_t open_file_fa485(ef_addr32 name, uint32_t flags, ...)
{
    va_list ap;
    uint32_t mode;

    va_start(ap, flags);
    mode = *(uint32_t *)ap[0];
    va_end(ap);
    return guest_fn_fa4a9_x(name, flags, 0, mode);
}

/* 0xa7b3c, 42 bytes. File size: open the named file in a handle slot, seek to its end, close it and return the end position, or -1 when it cannot be opened (stack argument, callee cleans up). */
int32_t file_size_a7b3c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 name)
{
    int32_t handle;
    int32_t size;
    (void)u0; (void)u1; (void)u2; (void)u3;
    handle = open_slot_a7900(0, 0, 0, 0, name);
    if (handle == -1)
        return handle;
    size = (int32_t)seek_end_a7ae0(0, 0, 0, 0, (uint32_t)handle, 0);
    close_handle_a7990(0, 0, 0, 0, (uint32_t)handle);
    return size;
}
