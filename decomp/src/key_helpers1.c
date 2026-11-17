/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* DOS error translation. */

/* 0x1101e8, 102 bytes. Store the low DOS error byte at 0x484480, translate through the signed-byte table at 0x17d5a8 (or use the high byte for extended errors), then store errno at 0x484484. Always returns -1. */
int32_t report_dos_error_1101e8(uint32_t code)
{
    uint8_t lo = (uint8_t)code;
    uint8_t idx = lo;
    uint32_t out;
    store_through_ebd36(lo);
    if (code < 0x100u) {
        if (EF_U8(0x00132a67u) >= 3u) {
            if (lo == 0x50u)
                idx = 0x0eu;
            else if (lo >= 0x22u)
                idx = 0x13u;
            else if (lo >= 0x20u)
                idx = 0x05u;
        }
        if (idx > 0x13u)
            idx = 0x13u;
        out = (uint32_t)(int32_t)(int8_t)EF_U8(0x0017d5a8u + idx);
    } else {
        out = (code >> 8) & 0xffu;
    }
    store_through_ebd30(out);
    return -1;
}
