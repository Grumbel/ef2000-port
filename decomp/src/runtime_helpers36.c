/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Stream line reader and record array release. */

/* 0x113398, 96 bytes. Release a record container: trace each 0x718-byte element, trace field 0xc, free the element array and the 0x1d4-byte container (cdecl, container on the stack). */
void release_record_array_113398(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 ctx)
{
    int32_t i = 0;
    uint32_t offset = 0;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_S32(ctx + 0x1cu) > 0) {
        do {
            trace_call_1031a0(EF_U32(ctx + 0x18u) + offset);
            i++;
            offset += 0x718u;
        } while (i < EF_S32(ctx + 0x1cu));
    }
    trace_call_102384(EF_U32(ctx + 0xcu));
    free_block_1047c4(EF_U32(ctx + 0x18u), EF_U32(ctx + 0x1cu) * 0x718u);
    free_block_1047c4(ctx, 0x1d4u);
}

/* 0x11580e, 99 bytes. fgets: read at most n-1 characters up to and including a newline into buf from stream; returns buf, or 0 on end of file or error before any character. */
ef_addr32 stream_fgets_11580e(ef_addr32 buf, int32_t n, ef_addr32 stream)
{
    uint32_t saved = EF_U32(stream + 0xcu) & 0x30u;
    ef_addr32 p = buf;
    int32_t c = 0; /* ORIGINAL BUG: the original leaves this uninitialised when n <= 1 */
    int32_t left = n;

    EF_U8(stream + 0xcu) &= 0xcf;
    for (;;) {
        left--;
        if (left <= 0)
            break;
        c = stream_fgetc_10f107(stream);
        if (c == -1)
            break;
        EF_U8(p) = (uint8_t)c;
        p++;
        if ((uint8_t)c == 0x0a)
            break;
    }
    if (c == -1 && (p == buf || (EF_U8(stream + 0xcu) & 0x20) != 0))
        buf = 0;
    else
        EF_U8(p) = 0;
    EF_U32(stream + 0xcu) |= saved;
    return buf;
}
