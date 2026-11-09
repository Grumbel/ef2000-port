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

/* 0x30b0f..0x30d55: LookAt and Dist use the same 0x9d55d decimal reader,
 * floating-point scale by the image's 32.0 constant and 0x9d72c truncation
 * as the previous vectors, but keep only the low 16 bits of each result.
 * LookAt writes three words at +0x7d; Dist writes four at +0x87. */
static void read_view_word_vector_30b0f(ef_addr32 stream, ef_addr32 rec,
                                       uint32_t field, uint32_t count,
                                       ef_addr32 scale_address)
{
    float scale = *(const float *)ef_guest_memory_ptr(scale_address);
    uint32_t i;
    for (i = 0; i < count; ++i) {
        double value;
        read_next_token_9d278(stream);
        value = guest_tactcom_view_decimal_9d55d(stream);
        EF_U16(rec + field + 2u * i) =
            (uint16_t)view_fistp_305e1((long double)value * (long double)scale);
    }
}

/* 0x30d55..0x30ead: each clamp enables its word flag and reads two
 * coordinates scaled by the original 182.0 float at 0x182a05. */
static void read_view_clamp_30d55(ef_addr32 stream, ef_addr32 rec,
                                  uint32_t field)
{
    EF_U16(rec + field) = 1u;
    read_view_word_vector_30b0f(stream, rec, field + 2u, 2u, 0x00182a05u);
}

/* 0x30ead..0x30f59: HUDCenter and HUDHeight use the original decimal
 * converter and truncation, but no extra floating-point scale. */
static void read_view_hud_word_30ead(ef_addr32 stream, ef_addr32 rec,
                                    uint32_t field)
{
    double value;
    read_next_token_9d278(stream);
    value = guest_tactcom_view_decimal_9d55d(stream);
    EF_U16(rec + field) = (uint16_t)view_fistp_305e1((long double)value);
}

/* 0x30f59..0x31068: Insts checks six independent layout labels. An
 * unrecognized value preserves the initialized +0x9b byte. These source
 * pointers include substrings of adjacent embedded string literals. */
static void read_view_instruments_30f59(ef_addr32 stream, ef_addr32 rec)
{
    ef_addr32 token;
    read_next_token_9d278(stream);
    token = stream + 0x400u;
    if (ascii_stricmp_9d524(token, 0x00182918u) == 0)
        EF_U8(rec + 0x9bu) = 5u;
    if (ascii_stricmp_9d524(token, 0x0018292cu) == 0)
        EF_U8(rec + 0x9bu) = 2u;
    if (ascii_stricmp_9d524(token, 0x0018293cu) == 0)
        EF_U8(rec + 0x9bu) = 3u;
    if (ascii_stricmp_9d524(token, 0x00182948u) == 0)
        EF_U8(rec + 0x9bu) = 4u;
    if (ascii_stricmp_9d524(token, 0x0018295cu) == 0)
        EF_U8(rec + 0x9bu) = 1u;
    if (ascii_stricmp_9d524(token, 0x0018296cu) == 0)
        EF_U8(rec + 0x9bu) = 0u;
}

/* 0x31068..0x310c8: Cockpit has an ON/other selector at +0xa0. */
static void read_view_cockpit_31068(ef_addr32 stream, ef_addr32 rec)
{
    read_next_token_9d278(stream);
    EF_U8(rec + 0xa0u) =
        ascii_stricmp_9d524(stream + 0x400u, 0x00182750u) == 0 ? 1u : 0u;
}

/* 0x310c8..0x311f9: END checks the InitialOffSet coordinate count (zero or
 * three). Camera mode two builds two 3-dword stack vectors, then computes
 * angle words +0x83/+0x85 and an unsigned 3D distance word at +0x87. */
static void finish_view_record_310f7(ef_addr32 frame, ef_addr32 stream,
                                     ef_addr32 rec)
{
    uint32_t i;
    uint32_t count = EF_U32(rec + 0x78u);
    if (count != 0u && count != 3u)
        guest_tactcom_view_count_boundary_31116(stream);
    if (EF_U8(rec + 0x3cu) == 2u) {
        uint32_t distances[3];
        for (i = 0; i < 3u; ++i) {
            EF_S32(frame + 0x70u + i * 4u) =
                (int32_t)EF_S16(rec + 0x7du + i * 2u);
            EF_U32(frame + 0x64u + i * 4u) = EF_U32(rec + 0x60u + i * 4u);
        }
        direction_angles_dc91c(frame + 0x80u, frame + 0x70u, frame + 0x64u);
        EF_U16(rec + 0x83u) = EF_U16(frame + 0x80u);
        EF_U16(rec + 0x85u) = EF_U16(frame + 0x82u);
        for (i = 0; i < 3u; ++i)
            distances[i] = abs_a28f2((int32_t)(EF_U32(frame + 0x70u + i * 4u) -
                                                    EF_U32(frame + 0x64u + i * 4u)));
        EF_U16(rec + 0x87u) = (uint16_t)length_3d_decc4(
            (int32_t)distances[0], (int32_t)distances[1], (int32_t)distances[2]);
    }
}

/* 0x304fc..0x311f9: Each recognized option returns to the original
 * 0x304fc physical-line loop. END finishes the record; unknown tokens
 * take the original three-argument error call at 0x310d8. */
static void read_view_text_options_304fc(ef_addr32 frame, ef_addr32 stream,
                                         ef_addr32 rec)
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
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828c4u) == 0) {
            read_next_token_9d278(stream);
            EF_U8(rec + 0x7cu) =
                ascii_stricmp_9d524(stream + 0x400u, 0x001828d0u) == 0 ? 0u : 1u;
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828d8u) == 0) {
            read_view_word_vector_30b0f(stream, rec, 0x7du, 3u, 0x00182a09u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828e0u) == 0) {
            read_view_word_vector_30b0f(stream, rec, 0x87u, 4u, 0x00182a09u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828e8u) == 0) {
            read_view_clamp_30d55(stream, rec, 0x8fu);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828f0u) == 0) {
            read_view_clamp_30d55(stream, rec, 0x95u);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x001828f8u) == 0) {
            read_view_hud_word_30ead(stream, rec, 0x9cu);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182904u) == 0) {
            read_view_hud_word_30ead(stream, rec, 0x9eu);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182910u) == 0) {
            read_view_instruments_30f59(stream, rec);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182974u) == 0) {
            read_view_cockpit_31068(stream, rec);
        } else if (ascii_stricmp_9d524(stream + 0x400u, 0x00182754u) == 0) {
            finish_view_record_310f7(frame, stream, rec);
            return;
        } else {
            guest_tactcom_view_unknown_option_boundary_310d8(stream);
        }
    }
}

/* 0x30214..0x3122b: Bounded ReadGenSeqs() prefix. The original opens the
 * constructed BG\\views\\views2.dat path at 0x9ccb8, reports a missing file
 * through the two-argument fatal callback at 0x132a74, or passes a successful
 * resource handle to the recovered 0x9d114 stream constructor. Its recovered
 * line/token helpers identify a '#' record start; this prefix initializes
 * that record, copies its bounded name, decodes its camera mode and handles
 * text, scalar, vector, look-at, clamp, HUD, instrument and cockpit options.
 * END validates and advances the slot index; non-'#' lines are skipped.
 * EOF calls the 0x9d088 stream release then the 0x9ce84 handle close,
 * returning to the next reader in apply_options_82cec. */
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
        for (;;) {
            int32_t index = EF_S16(frame + 0x90u);
            ef_addr32 rec;
            if (index >= 40)
                guest_tactcom_view_invalid_index_boundary_3120b(stream);
            read_text_line_9d1e8(stream);
            read_next_token_9d278(stream);
            if (ascii_stricmp_9d524(stream + 0x400u, 0x0018274cu) != 0) {
                if (EF_U8(stream) == 0u) {
                    guest_tactcom_view_release_stream_9d088(stream);
                    guest_tactcom_view_close_handle_9ce84(handle);
                    return;
                }
                continue;
            }
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
            read_view_text_options_304fc(frame, stream, rec);
            EF_U16(frame + 0x90u) = (uint16_t)(index + 1);
        }
    }
}

/* 0x2ff44..0x2ff7b: Initialize the twenty 0x9c-byte sequence-list slots.
 * This is a separate table from the forty 0xa1-byte generic camera records.
 * The original copies the five-byte default name to field +0x3c and writes
 * the default index word to +0x00, leaving all other bytes untouched. */
void init_sequence_list_slots_2ff44(void)
{
    uint16_t default_index = EF_U16(0x00182710u);
    uint32_t i;

    for (i = 0u; i < 20u; ++i) {
        ef_addr32 rec = 0x0019bf80u + i * 0x9cu;
        EF_U32(rec + 0x3cu) = EF_U32(0x00182708u);
        EF_U8(rec + 0x40u) = EF_U8(0x0018270cu);
        EF_U16(rec) = default_index;
    }
}

/* 0x300b0..0x300f5: Resolve a list entry against the forty generic camera
 * names. The original fatal callback at 0x300e6 handles missing names. */
static uint16_t lookup_sequence_camera_300b0(ef_addr32 stream)
{
    uint32_t i;
    for (i = 0u; i < 40u; ++i)
        if (ascii_stricmp_9d524(stream + 0x400u,
                                0x0019cbb0u + i * 0xa1u) == 0)
            return (uint16_t)i;
    guest_tactcom_sequence_missing_camera_300e6(stream);
    return UINT16_MAX;
}

/* 0x2ff44..0x30213: The second Views resource reader, reached immediately
 * after ReadGenSeqs() in apply_options_82cec. A '#' record has three string
 * lines, an ON/other line, and up to eight camera-name/scale/length lines
 * terminated by END. Each camera name resolves against the already loaded
 * 0xa1-byte camera records. Unsupported malformed input remains a boundary
 * in the host probe; the original does not bound these field copies. */
void read_sequence_lists_prefix_2ff44(ef_addr32 frame)
{
    uint32_t handle;
    ef_addr32 stream;
    uint16_t index = 0u;

    init_sequence_list_slots_2ff44();
    build_default_path_9c980(frame);
    EF_U16(frame + 0x78u) = 0u;
    strcat_9ca57(frame, 0x00182714u);
    handle = guest_fn_9ccb8(frame);
    EF_U32(frame + 0x64u) = handle;
    if (handle == UINT32_MAX)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00182724u, frame);
    stream = new_stream_9d114(handle);
    EF_U8(stream) = 0x63u;
    while (EF_U8(stream) != 0u && (int16_t)index < 20) {
        ef_addr32 rec;
        uint16_t entry = 0u;
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        if (ascii_stricmp_9d524(stream + 0x400u, 0x0018274cu) != 0)
            continue;

        rec = 0x0019bf80u + (uint32_t)index * 0x9cu;
        read_next_token_9d278(stream);
        guest_tactcom_sequence_string_guard_3001d(stream, 0x1eu, 0x3001du);
        copy_string_9d749(rec, stream + 0x400u);
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        read_next_token_9d278(stream);
        guest_tactcom_sequence_string_guard_3001d(stream, 0x1eu, 0x3003cu);
        copy_string_9d749(rec + 0x1eu, stream + 0x400u);
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        read_next_token_9d278(stream);
        guest_tactcom_sequence_string_guard_3001d(stream, 0x0fu, 0x3005bu);
        copy_string_9d749(rec + 0x3cu, stream + 0x400u);
        read_text_line_9d1e8(stream);
        read_next_token_9d278(stream);
        EF_U8(rec + 0x4bu) =
            ascii_stricmp_9d524(stream + 0x400u, 0x00182750u) == 0 ? 1u : 0u;

        for (;;) {
            double value;
            float scale;
            read_text_line_9d1e8(stream);
            read_next_token_9d278(stream);
            if (ascii_stricmp_9d524(stream + 0x400u, 0x00182754u) == 0) {
                if (entry >= 8u)
                    guest_tactcom_sequence_entry_boundary_3010e(stream);
                EF_U16(rec + 0x4cu + (uint32_t)entry * 2u) = UINT16_MAX;
                ++index;
                EF_U16(frame + 0x78u) = index;
                break;
            }
            if (EF_U8(stream) == 0u)
                guest_tactcom_sequence_entry_boundary_3010e(stream);
            if (entry >= 8u)
                guest_tactcom_sequence_entry_boundary_3010e(stream);
            EF_U16(rec + 0x4cu + (uint32_t)entry * 2u) =
                lookup_sequence_camera_300b0(stream);
            read_next_token_9d278(stream);
            value = guest_tactcom_view_decimal_9d55d(stream);
            scale = *(const float *)ef_guest_memory_ptr(0x0018278bu);
            EF_U16(rec + 0x60u + (uint32_t)entry * 2u) =
                (uint16_t)view_fistp_305e1((long double)value * (long double)scale);
            read_next_token_9d278(stream);
            value = guest_tactcom_view_decimal_9d55d(stream);
            scale = *(const float *)ef_guest_memory_ptr(0x0018278fu);
            EF_U32(rec + 0x74u + (uint32_t)entry * 4u) =
                (uint32_t)view_fistp_305e1((long double)value * (long double)scale);
            ++entry;
        }
    }

    guest_tactcom_view_release_stream_9d088(stream);
    guest_tactcom_view_close_handle_9ce84(handle);
    EF_U8(0x0019e5e0u) = 1u;
    EF_U8(0x0012db9eu) = 0x49u;
    EF_U16(0x0019e543u) = UINT16_MAX;
    EF_U8(0x0019e555u) = 1u;
    EF_U8(0x0019e5e3u) = 0u;
    EF_U8(0x0019e5dfu) = 0u;
    EF_U8(0x001a53dcu) = 0u;
    EF_U16(0x0019e5e5u) = EF_U16(0x001f71cau);
}
