/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sound object teardown, controller reset and stream refill. */

/* 0x117aa2, 44 bytes. Zero the buffer at +0x50 (length at +0x4c) of a record (cdecl, record on the stack). */
void clear_buffer_117aa2(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    uint32_t i;
    uint32_t length = EF_U32(rec + 0x4cu);
    ef_addr32 buf = EF_U32(rec + 0x50u);
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < length; i++)
        EF_U8(buf + i) = 0;
}

/* 0x105da0, 149 bytes. Destroy a sound object: if it was started, stop it, clear its buffer, run 0x117a0f twice, trace 0xa; then trace its field 0xc, free the channel table (count at +0x60, 0x894 bytes each), the buffer and the 0x8c-byte record (cdecl, record on the stack). */
void destroy_sound_object_105da0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 0x54u) != 0) {
        stop_object_105334(rec);
        clear_buffer_117aa2(0, 0, 0, 0, rec);
        guest_fn_117a0f_x(rec, 0);
        guest_fn_117a0f_x(rec, 1);
        trace_call_101ff4(0xa);
    }
    trace_call_102384(EF_U32(rec + 0xcu));
    nop_10483c();
    free_block_1047c4(EF_U32(rec + 0x5cu), EF_U32(rec + 0x60u) * 0x894u);
    free_block_1047c4(EF_U32(rec + 0x50u), EF_U32(rec + 0x4cu));
    free_block_1047c4(rec, 0x8cu);
}

/* 0x114058, 171 bytes. Reset the controllers of a state-4 sound record: set state 8, run 0x112808, then for each of its 16 channels send controller 0x40 / 0x70 (control change) events and clear or retrigger the per-channel entries whose values are 0x40 or more (cdecl, record on the stack). */
void reset_controllers_114058(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    ef_addr32 p;
    ef_addr32 owner;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec == 0 || EF_U32(rec + 4u) != 4u)
        return;
    EF_U32(rec + 4u) = 8;
    guest_fn_112808_x(rec);
    owner = EF_U32(rec);
    for (p = rec; p != rec + 0x40u; p += 4u) {
        uint32_t channel = EF_U32(p + 0x94u);
        if (EF_S32(p + 0x454u) >= 0x40)
            append_midi_event_111e40(0, 0, 0, 0, owner, channel | 0xb0u, 0x40u, 0);
        if (EF_S32(p + 0x1d4u) >= 0x40)
            EF_U32(owner + channel * 4u + 0x20u) = 0;
        if (EF_S32(p + 0x254u) >= 0x40)
            append_midi_event_111e40(0, 0, 0, 0, owner, channel | 0xb0u, 0x70u, 0);
        if (EF_S32(p + 0x194u) >= 0x40)
            trace_call_103b5c(owner, channel + 1u);
    }
}

/* 0x10f1bf, 171 bytes. Refill a stream buffer (_filbuf): allocate it if missing, flush stdout-style linked streams, then read from the handle (or take a pushed-back console character for an unbuffered console stream); sets EOF (0x10) or error (0x20) flags; returns the byte count in the stream's count field. */
int32_t stream_refill_10f1bf(ef_addr32 stream)
{
    uint32_t flags;
    int32_t count;

    if (EF_U32(stream + 8u) == 0)
        alloc_stream_buffer_10f071(stream);
    if ((EF_U8(stream + 0xdu) & 0x20) != 0 && (EF_U8(stream + 0xdu) & 6) != 0)
        count_matching_streams(0x2000u);
    flags = EF_U32(stream + 0xcu);
    EF_U32(stream) = EF_U32(stream + 8u);
    EF_U8(stream + 0xcu) &= 0xfb;
    if ((flags & 0x2400u) == 0x2400u && EF_U32(stream + 0x10u) == 0) {
        int32_t ch;
        EF_U32(stream + 4u) = 0;
        ch = (int32_t)guest_fn_11d63f_x();
        if (ch != -1) {
            EF_U8(EF_U32(stream)) = (uint8_t)ch;
            EF_U32(stream + 4u) = 1;
        }
    } else {
        uint32_t want;
        if ((EF_U8(stream + 0xdu) & 4) != 0)
            want = 1;
        else
            want = EF_U32(stream + 0x14u);
        EF_U32(stream + 4u) = guest_fn_10f0e8_x(EF_U32(stream + 0x10u), EF_U32(stream), want);
    }
    count = EF_S32(stream + 4u);
    if (count <= 0) {
        if (count == 0) {
            EF_U8(stream + 0xcu) |= 0x10;
        } else {
            EF_U32(stream + 4u) = 0;
            EF_U8(stream + 0xcu) |= 0x20;
        }
    }
    return EF_S32(stream + 4u);
}

/* 0x11402c, 42 bytes. Start a record that is not yet state 1: trace-call it, reset its table record and link, and set state 4 (cdecl, record on the stack). */
void start_record_11402c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec != 0 && EF_U32(rec + 4u) != 1u) {
        trace_call_103148(rec);
        reset_table_record_and_link_112bbc(0, 0, 0, 0, rec);
        EF_U32(rec + 4u) = 4;
    }
}
