/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small table helpers. */

/* 0xaa2f8, 85 bytes. Return 1 when the row (+0xb) or column (+0xc) bytes of a record fall outside the list limits, else 0. */
uint32_t position_out_of_range_aa2f8(ef_addr32 rec)
{
    uint32_t row = EF_U8(rec + 0xbu);
    uint32_t col = EF_U8(rec + 0xcu);
    uint32_t node = EF_U32(0x00419200u);
    uint32_t i;
    if (row >= EF_U16(0x004194c2u))
        return 1u;
    for (i = 0; i < row; i++)
        node = EF_U32(node);
    if (col >= EF_U16(node + 0xcu))
        return 1u;
    return 0u;
}

/* 0x10922c, 85 bytes. Lowercase a name, plant it as a sentinel entry of the 8-byte table at 0x4dd898 and store the value of the first matching entry at 0x4dd890. */
void select_table_entry_10922c(ef_addr32 name)
{
    uint32_t idx = EF_U32(0x004dd894u);
    uint32_t tbl = EF_U32(0x004dd898u);
    int32_t off = -8;
    lowercase_string(name);
    EF_U32(tbl + idx * 8u - 4u) = 0x0010922cu;
    EF_U32(tbl + idx * 8u - 8u) = name;
    do {
        off += 8;
    } while (guest_fn_a4380(EF_U32(EF_U32(0x004dd898u) + (uint32_t)off), name) != 0u);
    EF_U32(0x004dd890u) = EF_U32(EF_U32(0x004dd898u) + (uint32_t)off + 4u);
}
