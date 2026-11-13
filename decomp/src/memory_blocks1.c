/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Memory block size rounding. */

/* 0x10d94f, 119 bytes. Round the requested block size at *SIZE (EAX) up to a multiple of 4; zero stays zero (returns 0). Depending on the bytes 0x132a5e/0x132a5f either 8 is added or the 0x10d63b value is subtracted, the result is kept (and the call fails with 0 when adding 0x3c carries), raised to the dword at 0x17d400 (low bit cleared) and finally rounded up to a 4 KB multiple (a carry fails with 0). Returns 1 when the final size is non-zero. */
uint32_t align_block_size_10d94f(uint32_t *size)
{
    uint32_t value = (*size + 3u) & ~3u;
    uint32_t limit;

    if (value == 0u)
        return 0u;
    if ((EF_U8(0x00132a5eu) == 1u && EF_U8(0x00132a5fu) == 0u) || EF_U8(0x00132a5eu) == 9u)
        value += 8u;
    else
        value -= guest_fn_10d63b();
    *size = value;
    value += 0x3cu;
    if (value < *size)
        return 0u;
    limit = EF_U32(0x0017d400u);
    if (value < limit)
        value = limit & ~1u;
    *size = value;
    value += 0xfffu;
    if (value < *size)
        return 0u;
    value &= ~0xfffu;
    *size = value;
    return value != 0u;
}
