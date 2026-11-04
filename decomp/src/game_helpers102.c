/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record counting at a cell and printf conversion modifier parsing. */

/* 0xbfb2c, 91 bytes. Count the idle records at cell (x,y) that pass bcf18 and whose code is in the cc584 set or 10..15. */
uint32_t count_records_at_cell_bfb2c(int16_t x, int16_t y, ef_addr32 head)
{
    ef_addr32 rec = EF_U32(head);
    uint32_t n = 0;

    while (rec != 0) {
        if (EF_U8(rec + 0xbu) == 0
            && (int32_t)x == EF_S32(rec + 0x3bu)
            && (int32_t)y == EF_S32(rec + 0x3fu)
            && (uint16_t)guest_fn_bcf18_x(rec) != 0
            && ((uint16_t)code_in_set_cc584(EF_U8(rec + 0xcu)) != 0
                || (uint16_t)code_is_10_to_15(EF_U8(rec + 0xcu)) != 0))
            n++;
        rec = EF_U32(rec + 4u);
    }
    return n;
}

/* 0x1086f4, 217 bytes. Parse the width and size modifiers of a printf conversion ('*', digits, N/F, h/l/w/L) into the spec record; returns the advanced format pointer. */
ef_addr32 parse_format_modifiers_1086f4(ef_addr32 fmt, ef_addr32 spec)
{
    uint8_t flags;
    uint8_t c;

    flags = (uint8_t)((EF_U8(spec + 0x10u) | 1u) & 3u);
    EF_U32(spec + 0xcu) = 0xffffffffu;
    EF_U8(spec + 0x10u) = flags;
    if (EF_U8(fmt) == 0x2a) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) & 0xfeu);
        fmt++;
    }
    c = EF_U8(fmt);
    if (EF_U8(0x00195af0u + (uint8_t)(c + 1u)) & 0x20u) {
        uint32_t n = 0;
        do {
            n = n * 10u + c - 0x30u;
            fmt++;
            c = EF_U8(fmt);
        } while (EF_U8(0x00195af0u + (uint8_t)(c + 1u)) & 0x20u);
        EF_U32(spec + 0xcu) = n;
    }
    c = EF_U8(fmt);
    if (c == 0x4e) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) | 8u);
        fmt++;
    } else if (c == 0x46) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) | 4u);
        fmt++;
    }
    c = EF_U8(fmt);
    if (c == 0x68) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) | 0x10u);
        fmt++;
    } else if (c == 0x6c || c == 0x77) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) | 0x20u);
        fmt++;
    } else if (c == 0x4c) {
        EF_U8(spec + 0x10u) = (uint8_t)(EF_U8(spec + 0x10u) | 0x40u);
        fmt++;
    }
    return fmt;
}
