/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Serial divisor, stream putc and record destruction. */

/* 0x117e0c, 153 bytes. Program the serial port at obj+0x1c with a divisor derived from the rate (clamped 300..115200); returns the actual rate, or 0 when there is no port. */
int32_t set_serial_rate_117e0c(ef_addr32 obj, int32_t rate)
{
    int32_t divisor;
    int32_t actual;
    uint32_t base = EF_U32(obj + 0x1cu);

    if (base == 0)
        return 0;
    if (rate < 0x12c)
        rate = 0x12c;
    else if (rate > 0x1c200)
        rate = 0x1c200;
    divisor = 0x1c200 / rate;
    actual = 0x1c200 / divisor;
    disable_interrupts_11f2d3();
    guest_fn_117b0c(EF_U32(obj + 0x1cu) + 3u, 0x83u);
    guest_fn_117b0c(EF_U32(obj + 0x1cu) + 1u, (uint32_t)(divisor >> 8));
    guest_fn_117b0c(EF_U32(obj + 0x1cu), (uint32_t)divisor & 0xffu);
    guest_fn_117b0c(EF_U32(obj + 0x1cu) + 3u, 3u);
    enable_interrupts_11f2d5();
    return actual;
}

/* 0x10f43b, 164 bytes. fputc: store a character in the stream buffer (expanding newline to CR LF on text streams), flushing when full or line buffered; returns the character or -1. */
int32_t stream_fputc_10f43b(uint32_t c, ef_addr32 stream)
{
    uint32_t mask = 0x400u;

    if ((EF_U8(stream + 0xcu) & 2) == 0) {
        guest_fn_ebe6d(4);
        EF_U8(stream + 0xcu) |= 0x20;
        return -1;
    }
    if (EF_U32(stream + 8u) == 0)
        alloc_stream_buffer_10f071(stream);
    if (c == 10u) {
        mask = 0x600u;
        if ((EF_U8(stream + 0xcu) & 0x40) == 0) {
            EF_U8(stream + 0xdu) |= 0x10;
            EF_U8(EF_U32(stream)) = 0x0d;
            EF_U32(stream) += 1u;
            EF_U32(stream + 4u) += 1u;
            if (EF_U32(stream + 4u) == EF_U32(stream + 0x14u)) {
                if (guest_tail_fa8a0(stream) != 0)
                    return -1;
            }
        }
    }
    EF_U8(stream + 0xdu) |= 0x10;
    EF_U8(EF_U32(stream)) = (uint8_t)c;
    EF_U32(stream) += 1u;
    EF_U32(stream + 4u) += 1u;
    if ((mask & EF_U32(stream + 0xcu)) != 0 || EF_U32(stream + 4u) == EF_U32(stream + 0x14u)) {
        if (guest_tail_fa8a0(stream) != 0)
            return -1;
    }
    return (int32_t)(c & 0xffu);
}

/* 0x104464, 158 bytes. Destroy a record: run its field 0x24 callback with field 0x28, trace-call fields 0x20 and 0x1c, send message 0x306, free the 0x2c-byte record and drop it from the 16-entry table at 0x4dd670 (cdecl, record on the stack). */
void destroy_record_104464(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    uint32_t i;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 0x24u) != 0)
        guest_callptr1_x(EF_U32(rec + 0x24u), EF_U32(rec + 0x28u));
    if (EF_U32(rec + 0x20u) != 0xffffffffu)
        trace_call_102384(EF_U32(rec + 0x20u));
    if (EF_U32(rec + 0x18u) != 0) {
        if (EF_U32(rec + 0x1cu) != 0xffffffffu)
            trace_call_101f44(EF_U32(rec + 0x1cu));
        guest_fn_101f88(rec, 0x306u, 0, 0);
    }
    trace_call_101ed4(0x66u, EF_U32(EF_U32(rec + 0x10u) + 0x36u));
    nop_10483c();
    free_block_1047c4(rec, 0x2cu);
    for (i = 0; i != 0x40u; i += 4u) {
        if (rec == EF_U32(0x004dd670u + i))
            EF_U32(0x004dd670u + i) = 0;
    }
}
