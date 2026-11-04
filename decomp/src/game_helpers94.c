/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record list reactivation scan. */

/* 0xc10c4, 181 bytes. Reactivate up to count disabled (state 7) records at the given cell whose code passes the group and set tests; returns how many. */
uint32_t reactivate_records_at_cell_c10c4(int16_t x, int16_t y, uint32_t count, uint8_t type, ef_addr32 head_addr, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t n = 0;

    while (rec != 0 && n < (int16_t)count) {
        if (EF_U8(rec + 0xbu) == 0
            && (uint16_t)group_flag_positive_cc22c(EF_U8(rec + 0xeu), (uint32_t)type, head_addr) != 0
            && (int32_t)x == EF_S32(rec + 0x3bu)
            && (int32_t)y == EF_S32(rec + 0x3fu)
            && EF_U8(rec + 0xdu) == 7
            && ((uint16_t)code_in_set_cc584(EF_U8(rec + 0xcu)) != 0
                || (uint16_t)code_is_10_to_15(EF_U8(rec + 0xcu)) != 0)) {
            EF_U8(rec + 0xdu) = 0;
            n++;
            EF_U8(rec + 0x15u) = 100;
        }
        rec = EF_U32(rec + 4u);
    }
    return (uint32_t)(int32_t)n;
}
