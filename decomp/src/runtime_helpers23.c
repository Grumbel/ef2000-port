/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Driver query, screen blit and name lookup helpers, 0xc3714..0xcc744. */

typedef void (__stdcall *screen_blit_callback_t)(uint32_t, uint32_t, uint32_t);

/* 0xc3714, 81 bytes. Query the driver and return its two 16-bit status words. */
uint8_t query_driver_words_c3714(uint32_t request, ef_addr32 first, ef_addr32 second)
{
    if ((guest_fn_c3430(request) & 0xffu) != 0)
        guest_report_parser_enum_error(0x0018ee70u, request, 0);
    if ((((uint32_t (*)(void))EF_U32(0x0041f3c4u))() & 0xffu) == 0)
        guest_fatal_arg(0x0018ee4cu, request);
    EF_U16(first) = EF_U16(0x0041f3a8u);
    EF_U16(second) = EF_U16(0x0041f3aau);
    guest_fn_c3328();
    return 1;
}

/* 0xc3f1c, 52 bytes. Blit the screen buffer through the driver callback. */
void blit_screen_c3f1c(void)
{
    ((screen_blit_callback_t)EF_U32(0x0041f3d0u))(
        EF_U32(0x00164600u), EF_U32(0x0015b754u), EF_U32(0x00120cc6u) + 0xa0000u);
    if (EF_U8(0x00146e49u) != 0)
        guest_fn_c3a28();
}

/* 0xca780, 85 bytes. Find a name in the table of 25-byte entries at 0x13727e. */
uint32_t find_name_index_ca780(ef_addr32 name)
{
    ef_addr32 entry = 0x0013727eu;
    uint32_t index = 0;

    if (EF_U8(name) == 0)
        return 0;
    for (;;) {
        if (guest_string_length(entry) == 0) {
            guest_report_parser_enum_error(0x0018f2ecu, name, 0);
            return 0;
        }
        if (guest_fn_9d524(name, entry) == 0)
            return index;
        entry += 0x19u;
        index++;
    }
}

/* 0xcc744, 34 bytes. Allocate a block for a non-null request, aborting on failure. */
uint32_t alloc_checked_cc744(uint32_t size)
{
    uint32_t block;

    if (size == 0)
        return size;
    block = guest_fn_c3f78(size);
    if (block == 0)
        ui_report_missing_record(0x0018f41cu);
    return block;
}
