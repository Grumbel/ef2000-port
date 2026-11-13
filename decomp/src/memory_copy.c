/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0xdc832, 37 bytes. memcpy: copy COUNT bytes from SRC to DST (dwords first, then the tail bytes); returns DST. */
ef_addr32 memcpy_dc832(ef_addr32 dst, ef_addr32 src, uint32_t count)
{
    uint32_t i;
    uint32_t n = count >> 2;
    uint32_t d = dst;
    for (i = 0u; i < n; i++) {
        EF_U32(d) = EF_U32(src);
        d += 4u;
        src += 4u;
    }
    for (i = 0u; i < (count & 3u); i++) {
        EF_U8(d) = EF_U8(src);
        d++;
        src++;
    }
    return dst;
}
