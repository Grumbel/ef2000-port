/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb2d8c, 155 bytes. EAX record, EDX output; six other registers
 * preserved, plain RET. Sole caller b2f28 advances seven-byte records. */
void print_point_b2d8c(ef_addr32 rec, uint32_t out)
{
    int32_t last;
    int32_t previous;
    ef_addr32 action;
    ef_addr32 mission;
    uint32_t a, b, c;

    if (EF_U8(rec + 3u) == 0u) {
        last = EF_S32(rec + 5u) >> 16;
        previous = EF_S32(rec + 3u) >> 16;
        action = EF_U32(0x00132c2cu + (uint32_t)EF_U8(rec + 3u) * 4u);
        c = EF_U8(rec + 2u);
        b = EF_U8(rec + 1u);
        a = EF_U8(rec);
        print_to_handle_b1bb0(out, 0x0018e3e8u, a, b, c, action, previous, last);
    } else {
        last = EF_S32(rec + 5u) >> 16;
        previous = EF_S32(rec + 3u) >> 16;
        mission = EF_U32(0x00132e6cu + (uint32_t)EF_U8(rec + 4u) * 4u);
        action = EF_U32(0x00132c2cu + (uint32_t)EF_U8(rec + 3u) * 4u);
        c = EF_U8(rec + 2u);
        b = EF_U8(rec + 1u);
        a = EF_U8(rec);
        print_to_handle_b1bb0(out, 0x0018e418u, a, b, c, action, mission, previous, last);
    }
}
