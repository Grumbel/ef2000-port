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
 * parsed integer to a byte. The nested association sections at 0x1202+
 * remain an explicit host boundary, never an invented success return. */
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
    guest_tactcom_aid_next_section_1202(stream);
}
