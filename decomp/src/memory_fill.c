/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb3fb0, 24 bytes plus the original 0xdeb50 fill engine. Preserve
 * alignment prefix, repeated dword stores and trailing byte behavior.
 * No call into original code is needed by this replacement. */
void fill_bytes_b3fb0(ef_addr32 dst, uint8_t value, uint32_t count)
{
    uint32_t repeated = (uint32_t)value * 0x01010101u;
    while (count != 0u && (dst & 3u) != 0u) {
        EF_U8(dst++) = value;
        --count;
    }
    while (count >= 4u) {
        EF_U32(dst) = repeated;
        dst += 4u;
        count -= 4u;
    }
    while (count != 0u) {
        EF_U8(dst++) = value;
        --count;
    }
}
