/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table initialisers. */

/* 0x80d8c, 107 bytes. Allocate the 0xaf0-byte record pool, fill the 16-bit slot of each of its 200 14-byte records from a lookup on text, and run the name-entry init. */
void init_record_pool_80d8c(ef_addr32 text)
{
    uint32_t pool = arena_take_e29e8(0xaf0u);
    int32_t i;
    EF_U32(0x0012b0f8u) = pool;
    EF_U32(0x001fc960u) = pool;
    EF_U32(0x001fc964u) = pool;
    EF_U32(0x001fc968u) = pool + 0xaf0u;
    EF_U8(0x001fc970u) = 1u;
    for (i = 0; i < 200; i++)
        EF_U16(EF_U32(0x0012b0f8u) + (uint32_t)i * 14u + 0xcu) = (uint16_t)call_9de08_9ddfc(text);
    init_name_entries_80a18();
    EF_U32(0x00132adcu) = 0x00080f88u; /* EDX survives the call */
}

/* 0x9cfdc, 110 bytes. Select the two handler pairs for a name: defaults when it is empty, otherwise build the path string at 0x40d150 from 0x40d0c0, a separator and the name. */
void select_handlers_9cfdc(ef_addr32 name)
{
    if (name == 0u || EF_U8(name) == 0u) {
        EF_U32(0x00132ab4u) = 0x0009cb20u;
        EF_U32(0x00132ab8u) = 0x0009cbc4u;
        return;
    }
    EF_U32(0x00132ab4u) = 0x0009cf74u;
    EF_U32(0x00132ab8u) = 0x0009cfa4u;
    copy_string_9d749(0x0040d150u, 0x0040d0c0u);
    guest_fn_9ca57(0x0040d150u, 0x0018b1b4u);
    guest_fn_9ca57(0x0040d150u, name);
}
