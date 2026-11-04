/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Path splitting, name hashing and vector-by-matrix transform. */

/* 0xe7fc0, 263 bytes. _splitpath: normalise '/' to '\\' in place and split into drive, directory, name and extension (each output optional). */
void split_path_e7fc0(ef_addr32 path, ef_addr32 drive, ef_addr32 dir, ef_addr32 name, ef_addr32 ext)
{
    ef_addr32 p;
    ef_addr32 colon;
    ef_addr32 base;
    ef_addr32 dot;

    for (p = path; EF_U8(p) != 0; p++) {
        if (EF_U8(p) == 0x2f)
            EF_U8(p) = 0x5c;
    }
    colon = find_char(path, 0x3a);
    if (colon != 0) {
        colon++;
        if (drive != 0) {
            uint32_t n = colon - path;
            copy_padded(drive, path, n);
            EF_U8(drive + n) = 0;
        }
        path = colon;
    } else if (drive != 0) {
        EF_U8(drive) = 0;
    }
    base = find_last_char(path, 0x5c);
    if (base == 0)
        base = find_char(path, 0x3a);
    if (base == 0) {
        base = path;
        if (dir != 0)
            EF_U8(dir) = 0;
    } else {
        base++;
        if (dir != 0) {
            uint8_t saved = EF_U8(base);
            EF_U8(base) = 0;
            copy_string_9d749(dir, path);
            EF_U8(base) = saved;
        }
    }
    dot = find_last_char(base, 0x2e);
    if (dot == 0) {
        if (name != 0)
            copy_string_9d749(name, base);
        if (ext != 0)
            EF_U8(ext) = 0;
    } else {
        EF_U8(dot) = 0;
        if (name != 0)
            copy_string_9d749(name, base);
        EF_U8(dot) = 0x2e;
        if (ext != 0)
            copy_string_9d749(ext, dot);
    }
}

/* 0xea9ec, 184 bytes. Transform a 3-vector in place by a 3x3 1.15 matrix (each product divided by 0x8000 separately). */
void transform_vector_by_matrix_ea9ec(ef_addr32 vec, ef_addr32 matrix)
{
    int32_t v0 = EF_S32(vec);
    int32_t v1 = EF_S32(vec + 4u);
    int32_t v2 = EF_S32(vec + 8u);
    int32_t r[3];
    uint32_t j;

    for (j = 0; j < 3; j++) {
        r[j] = (int32_t)(((int64_t)v0 * EF_S32(matrix + j * 4u)) / 0x8000)
             + (int32_t)(((int64_t)v1 * EF_S32(matrix + 0xcu + j * 4u)) / 0x8000)
             + (int32_t)(((int64_t)v2 * EF_S32(matrix + 0x18u + j * 4u)) / 0x8000);
    }
    EF_S32(vec) = r[0];
    EF_S32(vec + 4u) = r[1];
    EF_S32(vec + 8u) = r[2];
}

/* 0xebfd0, 182 bytes. Hash up to the first 15 characters of a name into 32 bits (length, sum, running sum, mix and ordering bits; '_' is skipped). */
uint32_t name_hash_ebfd0(ef_addr32 text)
{
    int32_t len = (int32_t)guest_fn_b1b8b(text);
    uint8_t bits = 0;
    uint8_t mix = 0;
    uint8_t sum2 = 0;
    uint8_t sum1 = 0;
    uint8_t c = EF_U8(text);
    int16_t i = 0;

    if ((int16_t)len > 15)
        len = 15;
    if ((int16_t)len > 0) {
        do {
            if (i != 0 && c != 0x5f) {
                bits = (uint8_t)(bits + bits);
                bits = (uint8_t)(bits | (c < EF_U8(text + (uint32_t)(int32_t)i) ? 1u : 0u));
            }
            c = EF_U8(text + (uint32_t)(int32_t)i);
            if (c != 0x5f) {
                sum1 = (uint8_t)(sum1 + c);
                mix = (uint8_t)(mix + mix);
                sum2 = (uint8_t)(sum2 + sum1);
                mix = (uint8_t)(mix ^ c);
            } else {
                mix = (uint8_t)(mix - (uint8_t)i);
            }
            i++;
        } while (i < (int16_t)len);
    }
    return (((uint32_t)len & 0xfu) << 28) | (((uint32_t)sum1 & 0xfu) << 24) | ((uint32_t)sum2 << 16)
         | ((uint32_t)mix << 8) | (uint32_t)bits;
}
