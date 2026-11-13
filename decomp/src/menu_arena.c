/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa7b98, 21 bytes. Return the old cursor, then advance without alignment
 * or bounds checks. Unsigned addition preserves the original wraparound. */
ef_addr32 menu_allocate_a7b98(uint32_t size)
{
    ef_addr32 result = EF_U32(0x004191c8u);
    EF_U32(0x004191c8u) = result + size;
    return result;
}
