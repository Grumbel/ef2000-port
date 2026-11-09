/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Aid.c ReadAidData(), 0x00000f80..0x00001119. The original initializes
 * four unrelated tables before attempting to open views/aid2.dat. Keep the
 * original record strides and the five-byte default identifier copy. */
void init_aid_data_tables_f80(void)
{
    uint32_t i, row, col;

    for (i = 0; i < 27u; ++i)
        EF_U8(0x00196fcau + i) = 0u;

    for (i = 0; i < 8u; ++i)
        EF_U8(0x00196390u + i) = 0u;

    for (i = 0; i < 50u; ++i) {
        ef_addr32 rec = 0x00196398u + i * 0x3eu;
        EF_U32(rec) = EF_U32(0x0017f3bcu);
        EF_U8(rec + 4u) = EF_U8(0x0017f3c0u);
        EF_U8(rec + 0x32u) = 0u;
        EF_U8(rec + 0x33u) = 0u;
        EF_U16(rec + 0x34u) = 0u;
        EF_U32(rec + 0x36u) = 0u;
        EF_U32(rec + 0x3au) = 0u;
    }

    for (i = 0; i < 8u; ++i) {
        ef_addr32 rec = 0x001961f8u + i * 0x33u;
        EF_U8(rec) = 0u;
        for (row = 0; row < 5u; ++row)
            for (col = 0; col < 10u; ++col)
                EF_U8(rec + 1u + row * 10u + col) = 0xffu;
    }

    for (i = 0; i < 2u; ++i) {
        ef_addr32 rec = 0x00196190u + i * 0x34u;
        EF_U8(rec) = 0xffu;
        EF_U8(rec + 1u) = 0u;
        for (row = 0; row < 5u; ++row)
            for (col = 0; col < 10u; ++col)
                EF_U8(rec + 2u + row * 10u + col) = 0xffu;
    }
}

/* 0x1119..0x1202: file setup and the two fixed-size mapping arrays.
 * The first line in each block is skipped by the source, then its reader
 * obtains the *second* token from 27 or 8 following lines and truncates the
 * parsed integer to a byte. The next call enters nested associations. */

/* 0x1269..0x14bd: the two relationship fields use the same source labels
 * but the second permits NONE. Keep the non-numeric order of the original
 * enum (WINGMAN=5, THREAT=6, MTHREAT=7). */
static uint8_t aid_relationship_f80(ef_addr32 stream, uint32_t second)
{
    static const struct { ef_addr32 name; uint8_t value; } labels[] = {
        { 0x0017f3fcu, 1u }, { 0x0017f404u, 2u },
        { 0x0017f40cu, 3u }, { 0x0017f414u, 4u },
        { 0x0017f41cu, 6u }, { 0x0017f424u, 7u },
        { 0x0017f42cu, 5u }
    };
    uint32_t i;
    for (i = 0u; i < sizeof(labels) / sizeof(labels[0]); ++i)
        if (ascii_stricmp_9d524(stream + 0x400u, labels[i].name) == 0)
            return labels[i].value;
    if (second && ascii_stricmp_9d524(stream + 0x400u, 0x0017f3bcu) == 0)
        return 0u;
    guest_tactcom_aid_enum_boundary_f80(stream, second ? 0x000014a7u : 0x00001365u);
    return 0u; /* boundary does not return */
}

/* 0x14de..0x1573: 0x9d55d loads a decimal; source constants at 0x17f5b9
 * and 0x17f5bd are IEEE floats (32 and 50 in the original payload).
 * 0x9d72c sets x87 rounding toward zero before FISTP. The first two
 * destinations use the low dword of a signed qword; the third uses the low
 * word of a signed dword. */
static uint32_t aid_scaled_integer_f80(ef_addr32 stream,
                                       ef_addr32 scale_address, uint32_t word)
{
    float scale = *(const float *)ef_guest_memory_ptr(scale_address);
    long double value = (long double)guest_tactcom_view_decimal_9d55d(stream)
                      * (long double)scale;
    if (word) {
        if (value < -0x1p31L || value >= 0x1p31L)
            return (uint16_t)INT32_MIN;
        return (uint16_t)(int32_t)value;
    }
    if (value < -0x1p63L || value >= 0x1p63L)
        return (uint32_t)INT64_MIN;
    return (uint32_t)(int64_t)value;
}

/* 0x1202..0x15cb: consume two association headings, the first group and
 * option headings, and the first complete 0x3e-byte object. Its index is
 * written to the first cell of the group-1 0x33-byte grid; later group/row
 * transitions have not yet been reconstructed. The caller must stop at the
 * next source read-line boundary, with the stream and file still open. */
static void read_aid_first_association_f80(ef_addr32 stream)
{
    ef_addr32 rec = 0x00196398u;
    uint32_t value;

    read_text_line_9d1e8(stream); /* association heading 1 */
    guest_tactcom_aid_row_guard_f80(stream, 0x0000120eu);
    read_text_line_9d1e8(stream); /* association heading 2 */
    guest_tactcom_aid_row_guard_f80(stream, 0x00001217u);
    read_text_line_9d1e8(stream); /* group heading */
    guest_tactcom_aid_row_guard_f80(stream, 0x00001238u);
    read_text_line_9d1e8(stream); /* option heading */
    guest_tactcom_aid_row_guard_f80(stream, 0x00001244u);
    read_next_token_9d278(stream);

    read_text_line_9d1e8(stream); /* relation: keyword, source, target */
    guest_tactcom_aid_row_guard_f80(stream, 0x00001254u);
    read_next_token_9d278(stream);
    read_next_token_9d278(stream);
    EF_U8(rec + 0x32u) = aid_relationship_f80(stream, 0u);
    read_next_token_9d278(stream);
    EF_U8(rec + 0x33u) = aid_relationship_f80(stream, 1u);

    read_text_line_9d1e8(stream); /* two fixed-point values, ~ means zero */
    guest_tactcom_aid_row_guard_f80(stream, 0x000014c9u);
    read_next_token_9d278(stream);
    read_next_token_9d278(stream);
    EF_U32(rec + 0x36u) = aid_scaled_integer_f80(stream, 0x0017f5b9u, 0u);
    read_next_token_9d278(stream);
    value = EF_U8(stream + 0x400u) == '~' ? 0u :
        aid_scaled_integer_f80(stream, 0x0017f5b9u, 0u);
    EF_U32(rec + 0x3au) = value;

    read_text_line_9d1e8(stream); /* scaled 16-bit value */
    guest_tactcom_aid_row_guard_f80(stream, 0x00001544u);
    read_next_token_9d278(stream);
    read_next_token_9d278(stream);
    EF_U16(rec + 0x34u) = (uint16_t)aid_scaled_integer_f80(stream, 0x0017f5bdu, 1u);

    read_text_line_9d1e8(stream); /* identifier, copied without padding */
    guest_tactcom_aid_row_guard_f80(stream, 0x0000157cu);
    read_next_token_9d278(stream);
    read_next_token_9d278(stream);
    guest_tactcom_aid_name_guard_f80(stream);
    copy_string_9d749(rec, stream + 0x400u);
    EF_U8(0x001961f8u + 0x33u + 1u) = 0u;
    guest_tactcom_aid_next_section_15cb(stream);
}

void read_aid_data_prefix_f80(ef_addr32 frame)
{
    uint32_t handle, i;
    ef_addr32 stream;

    init_aid_data_tables_f80();
    build_default_path_9c980(frame);
    strcat_9ca57(frame, 0x0017f3c4u);
    handle = guest_fn_9ccb8(frame);
    EF_U32(frame + 0x70u) = handle;
    if (handle == UINT32_MAX)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0017f3d4u, frame);

    stream = new_stream_9d114(handle);
    read_text_line_9d1e8(stream); /* first mapping block heading */
    for (i = 0; i < 27u; ++i) {
        read_text_line_9d1e8(stream);
        guest_tactcom_aid_row_guard_f80(stream, 0x00001171u);
        read_next_token_9d278(stream);
        read_next_token_9d278(stream);
        EF_U8(0x00196fcau + i) = (uint8_t)parse_int(stream + 0x400u);
    }

    read_text_line_9d1e8(stream); /* second mapping block heading */
    for (i = 0; i < 8u; ++i) {
        read_text_line_9d1e8(stream);
        guest_tactcom_aid_row_guard_f80(stream, 0x000011cbu);
        read_next_token_9d278(stream);
        read_next_token_9d278(stream);
        EF_U8(0x00196390u + i) = (uint8_t)parse_int(stream + 0x400u);
    }
    read_aid_first_association_f80(stream);
}
