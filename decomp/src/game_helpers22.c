/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu setup helpers, 0x8fa84..0x97a4c. */

/* 0x8fa84, 60 bytes. Clear ten 24-byte menu entries and load their string handles. */
void init_menu_entries_8fa84(void)
{
    int16_t i;

    for (i = 0; i < 10; i++) {
        uint32_t entry = (uint32_t)((int32_t)i * 0x18);

        EF_U16(0x0020f474u + entry) = 0;
        EF_U16(0x0020f476u + entry) = (uint16_t)call_9de08_9ddfc(0x0018aa1cu);
    }
    EF_U16(0x002119f0u) = 0;
}

/* 0x8fd70, 55 bytes. Build a path from the default directory and three parts. */
void build_path_8fd70(ef_addr32 buffer, ef_addr32 first, ef_addr32 second, ef_addr32 third)
{
    copy_string_9d749(buffer, guest_fn_9c858(0x0018aa6cu));
    guest_fn_9ca57(buffer, first);
    guest_fn_9ca57(buffer, second);
    guest_fn_9ca57(buffer, third);
}

/* 0x97464, 81 bytes. Walk the 0x4e-byte chain of records from 0x12dc74 and draw those of depth >= 2. */
void draw_chain_97464(void)
{
    EF_U32(0x00132b20u) = 0x3e000000u;
    if (EF_U8(0x00137a00u) != 0) {
        int32_t index = (int16_t)EF_U16(0x0012dc74u);

        while (index != -1) {
            ef_addr32 record = 0x00212270u + (uint32_t)(index * 0x4e);

            if ((int16_t)EF_U16(record + 8u) < 2)
                break;
            guest_fn_978e8(record);
            index = (int16_t)EF_U16(record + 4u);
        }
        guest_fn_974b8();
    }
}

/* 0x97a4c, 69 bytes. Load two string handles and reset the chain head. */
void init_chain_97a4c(void)
{
    EF_U16(0x0021448cu) = (uint16_t)call_9de08_9ddfc(0x0018ac78u);
    EF_U16(0x0021448eu) = (uint16_t)call_9de08_9ddfc(0x0018ac7cu);
    EF_U16(0x0012dc76u) = 1; /* DX is preserved across the calls and still holds the 1 loaded before the first. */
    EF_U16(0x0012dc74u) = 0xffffu;
    EF_U8(0x001264e0u) = 0;
}
