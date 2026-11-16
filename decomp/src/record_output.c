/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb21ec, 140 bytes. EAX record, EDX output; six other registers
 * preserved, plain RET. Callers b21c7/b27d0 forward record and handle. */
void print_record_b21ec(ef_addr32 rec, uint32_t out)
{
    int32_t h0, h1, h2, h3, h4, h5;
    uint32_t b0, b1, b2, b3, b4;
    ef_addr32 name;

    if (rec != 0u) {
        h5 = EF_S32(rec + 0x12u) >> 16;
        h4 = EF_S32(rec + 0x10u) >> 16;
        h3 = EF_S32(rec + 0x0eu) >> 16;
        h2 = EF_S32(rec + 0x0cu) >> 16;
        h1 = EF_S32(rec + 0x0au) >> 16;
        h0 = EF_S32(rec + 0x08u) >> 16;
        b4 = EF_U8(rec + 9u);
        b3 = EF_U8(rec + 8u);
        b2 = EF_U8(rec + 7u);
        b1 = EF_U8(rec + 6u);
        b0 = EF_U8(rec + 5u);
        name = EF_U32(rec);
        print_to_handle_b1bb0(out, 0x0018e008u, name,
            b0, b1, b2, b3, b4, h0, h1, h2, h3, h4, h5);
    } else {
        print_to_handle_b1bb0(out, 0x0018e040u);
        print_to_handle_b1bb0(out, 0x0018e060u);
        print_to_handle_b1bb0(out, 0x0018e060u);
    }
}
