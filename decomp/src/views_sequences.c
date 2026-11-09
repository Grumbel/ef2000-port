/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x30214..0x3024e: Initialize the 40 generic sequence slots in Views.c.
 * The original copies exactly five bytes (including the name terminator)
 * from the default sequence name, then writes the default index word. */
void init_gen_sequence_slots_30214(void)
{
    uint16_t index = EF_U16(0x00182710u);
    uint32_t i;

    for (i = 0; i < 40u; ++i) {
        ef_addr32 offset = i * 0xa1u;
        EF_U32(0x0019cbf0u + offset) = EF_U32(0x00182708u);
        EF_U8(0x0019cbf4u + offset) = EF_U8(0x0018270cu);
        EF_U16(0x0019cbb0u + offset) = index;
    }
}

/* 0x302e1..0x303f9: Initialize the sequence record selected by the high
 * half of the original parser's stack-local index. Every write below is
 * present in this order in the original block; fields outside these offsets
 * retain the initial image's contents. The record name and options are read
 * after this block and are not recovered here. */
static void init_view_record_302e1(uint32_t index)
{
    ef_addr32 rec = 0x0019cbb0u + index * 0xa1u;

    EF_U32(rec + 0x1eu) = EF_U32(0x001827d0u);
    EF_U32(rec + 0x22u) = EF_U32(0x001827d4u);
    EF_U8(rec + 0x3du) = 0u;
    EF_U8(rec + 0x3eu) = 0x64u;
    EF_U8(rec + 0x3fu) = 0x64u;
    EF_U16(rec + 0x8fu) = 0u;
    EF_U16(rec + 0x95u) = 0u;
    EF_U16(rec + 0x81u) = 0u;
    EF_U32(rec + 0x4fu) = 0xfffffd00u;
    EF_U32(rec + 0x60u) = 0u;
    EF_U32(rec + 0x64u) = 0u;
    EF_U32(rec + 0x68u) = 0u;
    EF_U16(rec + 0x9cu) = 0x63u;
    EF_U8(rec + 0x53u) = 3u;
    EF_U16(rec + 0x7du) = 0u;
    EF_U8(rec + 0x7cu) = 0u;
    EF_U8(rec + 0x9bu) = 0u;
    EF_U16(rec + 0x7fu) = 0u;
    EF_U16(rec + 0x9eu) = 0xc7u;
    EF_U32(rec + 0x40u) = EF_U32(0x00182708u);
    EF_U8(rec + 0x44u) = EF_U8(0x0018270cu);
    EF_U8(rec + 0xa0u) = 1u;
    EF_U32(rec + 0x78u) = 0u;
    EF_U16(rec + 0x89u) = 0x140u;
    EF_U16(rec + 0x8bu) = 0x640u;
    EF_U16(rec + 0x87u) = 0x280u;
    EF_U16(rec + 0x8du) = 0x40u;
    EF_U32(rec + 0x54u) = 0u;
    EF_U32(rec + 0x58u) = 0u;
    EF_U32(rec + 0x5cu) = 0u;
    EF_U16(rec + 0x83u) = 0u;
    EF_U16(rec + 0x85u) = 0u;
}

/* 0x30423..0x304fc: The six independent, case-insensitive camera-mode
 * comparisons from the first option line. They use the second token of
 * that line, leaving the +0x3c field alone when none match. */
static void set_view_camera_mode_30423(ef_addr32 rec, ef_addr32 token)
{
    if (ascii_stricmp_9d524(token, 0x001827d8u) == 0)
        EF_U8(rec + 0x3cu) = 0u;
    if (ascii_stricmp_9d524(token, 0x001827e0u) == 0)
        EF_U8(rec + 0x3cu) = 1u;
    if (ascii_stricmp_9d524(token, 0x001827f0u) == 0)
        EF_U8(rec + 0x3cu) = 2u;
    if (ascii_stricmp_9d524(token, 0x001827fcu) == 0)
        EF_U8(rec + 0x3cu) = 3u;
    if (ascii_stricmp_9d524(token, 0x00182810u) == 0)
        EF_U8(rec + 0x3cu) = 4u;
    if (ascii_stricmp_9d524(token, 0x0018281cu) == 0)
        EF_U8(rec + 0x3cu) = 5u;
}

/* 0x305e1..0x306e3: The first three numeric options use the original
 * 0x9d55d decimal reader (bounded by a host adapter for this GCC probe),
 * 0x9d72c truncation and x87 FISTP. Keep the original float constants
 * resident in the loaded image rather than duplicating them here. */
static int32_t view_fistp_305e1(long double value)
{
    if (!(value >= -2147483648.0L && value < 2147483648.0L))
        return INT32_MIN;
    return (int32_t)value;
}

static void read_view_numeric_token_305e1(ef_addr32 stream, ef_addr32 rec,
                                          uint32_t option)
{
    float scale;
    double value;
    int32_t result;

    read_next_token_9d278(stream);
    value = guest_tactcom_view_decimal_9d55d(stream);
    if (option == 0u || option == 1u) {
        scale = *(const float *)ef_guest_memory_ptr(0x00182a0du);
        result = view_fistp_305e1((long double)scale - (long double)value);
        EF_U8(rec + 0x3eu + option) = (uint8_t)result;
    } else {
        scale = *(const float *)ef_guest_memory_ptr(0x00182a09u);
        result = view_fistp_305e1(-(long double)value * (long double)scale);
        EF_U32(rec + 0x4fu) = (uint32_t)result;
    }
}

/* 0x306e3..0x307f2: RollLock compares six independent mode labels, all
 * taken from the original embedded strings. An unknown value retains the
 * already initialized record byte at +0x53. */
static void read_view_roll_lock_306e3(ef_addr32 stream, ef_addr32 rec)
{
    ef_addr32 token;
    read_next_token_9d278(stream);
    token = stream + 0x400u;
    if (ascii_stricmp_9d524(token, 0x00182708u) == 0)
        EF_U8(rec + 0x53u) = 1u;
    if (ascii_stricmp_9d524(token, 0x00182874u) == 0)
        EF_U8(rec + 0x53u) = 3u;
    if (ascii_stricmp_9d524(token, 0x0018287cu) == 0)
        EF_U8(rec + 0x53u) = 2u;
    if (ascii_stricmp_9d524(token, 0x0018288cu) == 0)
        EF_U8(rec + 0x53u) = 4u;
    if (ascii_stricmp_9d524(token, 0x00182894u) == 0)
        EF_U8(rec + 0x53u) = 5u;
    if (ascii_stricmp_9d524(token, 0x00182840u) == 0)
        EF_U8(rec + 0x53u) = 0u;
}

/* 0x307f2..0x30aaf: OffSet, RandomOffset and InitialOffSet parse three
 * decimal numbers through the 0x9d55d adapter, multiply by the source
 * float constant 32, and truncate to signed dwords. InitialOffSet also
 * increments the record's +0x78 dword by three after the last token. */
static void read_view_vector_307f2(ef_addr32 stream, ef_addr32 rec,
                                   uint32_t field, uint32_t increment)
{
    float scale = *(const float *)ef_guest_memory_ptr(0x00182a09u);
    uint32_t i;
    for (i = 0; i < 3u; ++i) {
        double value;
        read_next_token_9d278(stream);
        value = guest_tactcom_view_decimal_9d55d(stream);
        EF_U32(rec + field + 4u * i) =
            (uint32_t)view_fistp_305e1((long double)value * (long double)scale);
    }
    if (increment)
        EF_U32(rec + 0x78u) += 3u;
}

/* 0x304fc..0x30aaf: Each recognized option returns to the original
 * 0x304fc physical-line loop. The next unsupported option is LookAtObj. */
static void read_view_text_options_304fc(ef_addr32 stream, ef_addr32 rec)
{
    for (;;) {
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        if (ascii_stricmp_9d524(stream + 0x400u, 0x00182828u) == 0) {
            read_next_token_9d278(stream);
            guest_tactcom_view_string_guard_3053d(stream, 0x1eu);
            copy_string_9d749(rec + 0x1eu, stream + 0x400u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182830u) == 0) {
            read_next_token_9d278(stream);
            guest_tactcom_view_string_guard_30577(stream, 0x0fu);
            copy_string_9d749(rec + 0x40u, stream + 0x400u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182834u) == 0) {
            read_next_token_9d278(stream);
            EF_U8(rec + 0x3du) =
                ascii_stricmp_9d524(stream + 0x400u, 0x00182840u) == 0 ? 0u : 1u;
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182848u) == 0) {
            read_view_numeric_token_305e1(stream, rec, 0u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182854u) == 0) {
            read_view_numeric_token_305e1(stream, rec, 1u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182860u) == 0) {
            read_view_numeric_token_305e1(stream, rec, 2u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182868u) == 0) {
            read_view_roll_lock_306e3(stream, rec);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x0018289cu) == 0) {
            read_view_vector_307f2(stream, rec, 0x60u, 0u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828a4u) == 0) {
            read_view_vector_307f2(stream, rec, 0x54u, 0u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828b4u) == 0) {
            read_view_vector_307f2(stream, rec, 0x6cu, 1u);
        } else {
            guest_tactcom_view_lookat_boundary_30aaf(stream);
        }
    }
}

/* 0x30214..0x30aaf: Bounded ReadGenSeqs() prefix. The original opens the
 * constructed BG\\views\\views2.dat path at 0x9ccb8, reports a missing file
 * through the two-argument fatal callback at 0x132a74, or passes a successful
 * resource handle to the recovered 0x9d114 stream constructor. Its recovered
 * line/token helpers identify a '#' record start; this prefix initializes
 * that record, copies its bounded name, decodes its camera mode and handles
 * text, scalar and first vector options before stopping at LookAtObj. */
void read_gen_sequences_prefix_30214(ef_addr32 frame)
{
    uint32_t handle;
    ef_addr32 stream;

    init_gen_sequence_slots_30214();
    build_default_path_9c980(frame);
    EF_U16(frame + 0x90u) = 0u;
    strcat_9ca57(frame, 0x00182794u);
    handle = guest_fn_9ccb8(frame);
    EF_U32(frame + 0x84u) = handle;
    if (handle == UINT32_MAX)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x001827a8u, frame);
    else {
        stream = new_stream_9d114(handle);
        EF_U8(stream) = 0x63u;
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        if (ascii_stricmp_9d524(stream + 0x400u, 0x0018274cu) != 0)
            guest_tactcom_view_nonrecord_boundary_31201(stream);
        else {
            int32_t index = EF_S16(frame + 0x90u);
            ef_addr32 rec;
            if (index < 0 || index >= 40)
                guest_tactcom_view_invalid_index_boundary_3120b(stream);
            init_view_record_302e1((uint32_t)index);
            rec = 0x0019cbb0u + (uint32_t)index * 0xa1u;
            read_next_token_9d278(stream);
            guest_tactcom_view_name_guard_30407(stream);
            copy_string_9d749(rec, stream + 0x400u);
            read_text_line_9d1e8(stream);
            read_next_token_9d278(stream);
            read_next_token_9d278(stream);
            set_view_camera_mode_30423(rec, stream + 0x400u);
            read_view_text_options_304fc(stream, rec);
        }
    }
}
