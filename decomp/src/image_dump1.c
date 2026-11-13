/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Image rows written to the log file. */

/* Take SIZE bytes from the stack (the original's "sub esp, eax; mov eax, esp"), without Watcom's stack-grow checks. */
extern uint32_t stack_take(uint32_t size);
#pragma aux stack_take = "sub esp, eax" "mov eax, esp" parm [eax] value [eax] modify exact [eax esp];

/* 0xd1afc, 237 bytes. Write the image (width dword 0x15b744, height dword 0x15b748, group count dword 0x15b7cc) to the log file row by row: header text 0x18f734 and the dword 0x10040, then for every row one pixel row is read with 0xc3f50, packed into bit planes with 0xd19ec and written with 0xd16d0. The two row buffers are taken from the stack (a buffer is refused, and the error handler 0x132a74 called with 0x18f714, when it would not fit the stack space reported by 0xdcb74); the number of bytes written is kept in dword 0x420940. */
void write_planar_image_d1afc(void)
{
    ef_addr32 row = 0u;
    ef_addr32 packed = 0u;
    uint32_t row_size = (EF_U32(0x0015b744u) + 3u) & ~3u;
    uint32_t packed_size = ((EF_U32(0x0015b7ccu) + 1u) * 8u + 0x3bu) & ~3u;
    int32_t y;
    uint32_t total;

    if (row_size < stack_used_dcb74())
        row = stack_take(row_size);
    if (packed_size < stack_used_dcb74())
        packed = stack_take(packed_size);
    if (row == 0u || packed == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f714u);
    call_d16d0_d16fc(0x0018f734u);
    write_log_dword_swapped_d170c(0x00010040u);
    total = 0u;
    EF_U32(0x00420940u) = 0u;
    for (y = 0; y < (int32_t)EF_U32(0x0015b748u); y++) {
        int32_t x;
        uint32_t count;

        for (x = 0; x < (int32_t)EF_U32(0x0015b744u); x++)
            EF_U8(row + (uint32_t)x) = (uint8_t)guest_fn_c3f50(x, y);
        count = pack_bit_planes_d19ec(packed, row);
        write_log_d16d0(packed, count);
        total = EF_U32(0x00420940u) + count;
        EF_U32(0x00420940u) = total;
    }
}
