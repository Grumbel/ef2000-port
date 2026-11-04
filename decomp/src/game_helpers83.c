/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Buffer hex dump and temp file name generation. */

/* 0xb3580, 96 bytes. Hex dump 0x640 bytes through the printf sink, 40 values per line. */
void dump_buffer_b3580(ef_addr32 buffer, uint32_t handle, uint32_t title)
{
    int16_t i = 0;

    guest_fn_b1bb0(handle, 0x0018dee0u, title);
    while (i < 0x640) {
        if ((int32_t)i % 40 == 0 && i > 0)
            guest_fn_b1bb0(handle, 0x0018e060u);
        guest_fn_b1bb0(handle, 0x0018e66cu, (uint32_t)EF_U8(buffer + (uint32_t)(int32_t)i));
        i++;
    }
}

/* 0xbc471, 105 bytes. Build a temp file name "t" + four hex digits + "_" + two hex digits + ".tmp". */
void make_temp_name_bc471(ef_addr32 name, uint32_t index)
{
    uint32_t id = guest_fn_fa89a_x();
    uint32_t pos;

    EF_U8(name) = 't';
    for (pos = 4; pos != 0; pos--) {
        EF_U8(name + pos) = (uint8_t)guest_fn_bc465_x(id & 0xfu);
        id >>= 4;
    }
    EF_U8(name + 5u) = '_';
    EF_U8(name + 6u) = (uint8_t)guest_fn_bc465_x((index >> 4) & 0xfu);
    EF_U8(name + 8u) = '.';
    EF_U8(name + 9u) = 't';
    EF_U8(name + 0xau) = 'm';
    EF_U8(name + 0xbu) = 'p';
    EF_U8(name + 0xcu) = 0;
    EF_U8(name + 7u) = (uint8_t)guest_fn_bc465_x(index & 0xfu);
}
