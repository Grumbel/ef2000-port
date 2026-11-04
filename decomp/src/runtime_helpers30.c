/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Text cursor scanners and run-length image row decoder. */

/* 0x1092b0, 119 bytes. Skip blanks, parse a decimal number at *cursor, skip blanks again and store the new cursor. */
uint32_t parse_decimal_1092b0(ef_addr32 cursor)
{
    ef_addr32 cur = EF_U32(cursor);
    uint32_t value = 0;

    skip_class_chars((uint32_t)(uintptr_t)&cur);
    while (EF_U8(0x00195af0u + (uint8_t)(EF_U8(cur) + 1u)) & 0x20u) {
        value = value * 10u + EF_U8(cur) - 0x30u;
        cur++;
    }
    skip_class_chars((uint32_t)(uintptr_t)&cur);
    EF_U32(cursor) = cur;
    return value;
}

/* 0x10934c, 138 bytes. Skip blanks and copy an identifier (class bits 0xe0 or '_') at *cursor into dest, NUL-terminated. */
void copy_identifier_10934c(ef_addr32 dest, ef_addr32 cursor)
{
    ef_addr32 cur = EF_U32(cursor);

    skip_class_chars((uint32_t)(uintptr_t)&cur);
    if ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(cur) + 1u)) & 0xe0u) != 0 || EF_U8(cur) == 0x5f) {
        do {
            EF_U8(dest) = EF_U8(cur);
            cur++;
            dest++;
        } while ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(cur) + 1u)) & 0xe0u) != 0 || EF_U8(cur) == 0x5f);
        skip_class_chars((uint32_t)(uintptr_t)&cur);
    }
    EF_U8(dest) = 0;
    EF_U32(cursor) = cur;
}

/* 0x110894, 151 bytes. Decode run-length coded rows: each row starts with a run count, each run is a signed length (negative = literal bytes, positive = repeated byte). */
void decode_rle_rows_110894(ef_addr32 src, ef_addr32 dst, int16_t stride, int16_t rows)
{
    int16_t row = 0;

    while (row < rows) {
        uint8_t runs = EF_U8(src);
        ef_addr32 row_start = dst;
        int32_t r = 0;

        src++;
        while ((int16_t)runs > (int16_t)r) {
            int8_t len = (int8_t)EF_U8(src);
            src++;
            if (len < 0) {
                int16_t k = 0;
                while ((int32_t)k < -(int32_t)len) {
                    EF_U8(dst) = EF_U8(src);
                    dst++;
                    src++;
                    k++;
                }
            } else {
                uint8_t v = EF_U8(src);
                int16_t k = 0;
                src++;
                while (k < (int16_t)len) {
                    EF_U8(dst) = v;
                    dst++;
                    k++;
                }
            }
            r++;
        }
        dst = row_start + (uint32_t)(int32_t)stride;
        row++;
    }
}
