/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* printf conversion specification parser (output side). */

/* 0xed3b4, 307 bytes. Parse flags (ed4e7), width ('*' or digits), precision ('.' then '*' or digits) and size modifier (N F L h l w) into the spec record; returns the advanced format pointer. */
ef_addr32 parse_output_spec_ed3b4(ef_addr32 fmt, ef_addr32 va_ptr, ef_addr32 spec)
{
    uint8_t c;

    EF_U8(spec + 0x17u) = 0;
    EF_U8(spec + 0x16u) = 0x20;
    fmt = guest_fn_ed4e7_x(fmt, spec);
    EF_U32(spec + 4u) = 0;
    c = EF_U8(fmt);
    if (c >= 0x30 && c <= 0x39) {
        do {
            EF_U32(spec + 4u) = EF_U32(spec + 4u) * 10u + EF_U8(fmt) - 0x30u;
            fmt++;
            c = EF_U8(fmt);
        } while (c >= 0x30 && c <= 0x39);
    } else if (EF_U8(fmt) == 0x2a) {
        uint32_t arg;

        EF_U32(va_ptr) = EF_U32(va_ptr) + 4u;
        arg = EF_U32(EF_U32(va_ptr) - 4u);
        EF_U32(spec + 4u) = arg;
        if ((int32_t)arg < 0) {
            EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 8u);
            EF_U32(spec + 4u) = 0u - arg;
        }
        fmt++;
    }
    EF_U32(spec + 8u) = 0xffffffffu;
    if (EF_U8(fmt) == 0x2e) {
        fmt++;
        EF_U32(spec + 8u) = 0;
        if (EF_U8(fmt) == 0x2a) {
            uint32_t arg;

            EF_U32(va_ptr) = EF_U32(va_ptr) + 4u;
            arg = EF_U32(EF_U32(va_ptr) - 4u);
            EF_U32(spec + 8u) = arg;
            if ((int32_t)arg < 0)
                EF_U32(spec + 8u) = 0xffffffffu;
            fmt++;
        } else {
            for (;;) {
                c = EF_U8(fmt);
                if (c < 0x30 || c > 0x39)
                    break;
                EF_U32(spec + 8u) = EF_U32(spec + 8u) * 10u + EF_U8(fmt) - 0x30u;
                fmt++;
            }
        }
    }
    c = EF_U8(fmt);
    if (c == 0x4e || c == 0x4c) {
        EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 0x40u);
        fmt++;
    } else if (c == 0x46) {
        EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 0x80u);
        fmt++;
    } else if (c == 0x68) {
        EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 0x10u);
        fmt++;
    } else if (c == 0x6c || c == 0x77) {
        EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 0x20u);
        fmt++;
    }
    return fmt;
}
