/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Entry, wait-list and list-search helpers (parser file cursor, driver table), 0x3ce70..0xfddf0. */

/* 0x3ce70, 149 bytes. Resolve the terrain cell of every 11-byte entry that has none yet. */
void resolve_entry_cells_3ce70(void)
{
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(0x001a5aa0u); i++) {
        ef_addr32 entry = 0x001a57d1u + (uint32_t)((int32_t)i * 11);

        if (EF_U8(entry + 9u) == 0) {
            int32_t cell = (int16_t)guest_fn_8b9e8(EF_U8(entry), EF_U8(entry + 1u));

            if (cell == -1)
                cell = (int16_t)read_plane_cell(0, EF_U8(entry), EF_U8(entry + 1u));
            if (cell == -1)
                guest_report_parser_enum_error(0x001837e8u, (uint32_t)((int32_t)i + 1), 0);
            EF_U16(entry + 6u) = (uint16_t)cell;
        }
    }
}

/* 0xf478c, 100 bytes. Run the handler of the table entry whose name matches the current one. */
void run_matching_handler_f478c(void)
{
    uint8_t done = 0;
    int32_t i = 0;

    do {
        ef_addr32 name = EF_U32(0x00161e34u + (uint32_t)((int32_t)(int16_t)i * 8));

        if (name != 0) {
            if (guest_fn_9d524(name, 0x00485e08u) == 0) {
                ((void (*)(uint32_t, uint8_t))EF_U32(0x00161e38u + (uint32_t)((int32_t)(int16_t)i * 8)))(0, 1);
                done = 1;
            } else {
                i++;
            }
        } else {
            guest_report_parser_enum_error(0x00191c74u, 0x00485e08u, EF_U32(0x00161ea8u));
        }
    } while (done == 0);
}

/* 0xfddf0, 46 bytes. Store the lower and upper bound of a range as 16.16 values and notify. */
void store_range_fddf0(ef_addr32 center, uint32_t radius)
{
    EF_U32(0x00485fe8u) = (EF_U32(center) - radius) << 16;
    EF_U32(0x00485fd0u) = (radius + EF_U32(center)) << 16;
    ((void (*)(void))EF_U32(0x0017cf90u))();
    EF_U16(0x00485feeu) = (uint16_t)(EF_U16(0x00485feeu) + 1u);
}
