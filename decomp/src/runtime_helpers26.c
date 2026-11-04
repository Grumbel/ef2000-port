/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Data path building, checked allocation and driver restart helpers, 0xeabe8..0xf2dfc.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0xeabe8, 93 bytes. Build the data file path from the default path, a name and a suffix. */
void build_data_path_eabe8(ef_addr32 name)
{
    build_default_path_9c980(0x004644f0u);
    guest_fn_9ca57(0x004644f0u, 0x0019157cu);
    guest_fn_9ca57(0x004644f0u, name);
    guest_fn_9ca57(0x004644f0u, 0x00191584u);
    if (((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(0x004644f0u) == 0)
        guest_report_parser_enum_error(0x0019158cu, 0x004644f0u, 0);
}

/* 0xeb4a0, 30 bytes. Allocate a block or abort. */
uint32_t alloc_block_checked_eb4a0(uint32_t size)
{
    uint32_t block = guest_fn_c3f70(size);

    if (block == 0)
        ui_report_missing_record(0x001915e4u);
    return block;
}

/* 0xf1850, 81 bytes. Copy an entry's name when its kind is 1 and publish the kind. */
void select_entry_kind_f1850(ef_addr32 entry)
{
    if ((EF_U8(entry + 0x8bu) & 0xfu) == 1) {
        if (guest_string_length(EF_U32(entry + 0x87u)) > 0x20)
            ui_report_missing_record(0x00191888u);
        copy_string_9d749(0x0041f262u, EF_U32(entry + 0x87u));
    }
    EF_U16(0x0041eed2u) = (uint16_t)(EF_U8(entry + 0x8bu) & 0xfu);
}

/* 0xf2dfc, 86 bytes. Save the current path, restart the driver and abort when it fails. */
void restart_driver_f2dfc(void)
{
    uint8_t saved[0x64];

    guest_fn_f2cd0();
    copy_string_9d749((ef_addr32)(uintptr_t)saved, 0x0048592cu);
    guest_fn_f2dcc((ef_addr32)(uintptr_t)saved);
    guest_fn_c3974();
    if ((uint16_t)guest_fn_c34cc_ret(0x0048592cu) == 0) {
        guest_fn_c3f80(EF_U32(0x00485928u));
        guest_report_parser_enum_error(0x00191900u, 0x0048592cu, 0);
    }
}
