/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Screen copy, event dispatch and DOS clock helpers. */

/* 0x110b30, 61 bytes. Copy 200 lines of 0x140 bytes with the destination advancing by a 16-bit stride per line. */
void copy_screen_lines_110b30(ef_addr32 src, ef_addr32 dst, int16_t stride)
{
    int32_t line;

    for (line = 0; line < 200; line++) {
        guest_fn_dc832(dst, src, 0x140);
        src += 0x140;
        dst += (uint32_t)(int32_t)stride;
    }
}

/* 0x1e0ec, 68 bytes. Look up an event code for a record (position >> 8 as a 3-vector) and dispatch it unless the lookup returns -1. */
void dispatch_record_event_1e0ec(ef_addr32 rec, int16_t value)
{
    int32_t vec[3];
    int16_t code;

    vec[0] = EF_S32(rec + 9u) >> 8;
    vec[1] = EF_S32(rec + 0xdu) >> 8;
    vec[2] = EF_S32(rec + 0x11u) >> 8;
    code = (int16_t)guest_fn_39548_x(rec, 0xf, (int32_t)value);
    if (code != -1)
        guest_fn_1d5cc((uint32_t)(uintptr_t)vec, (int32_t)code);
}

/* 0x11004b, 91 bytes. DOS get-time call (int 21h, AH=2Ch) converted to hundredths of a second since midnight. */
uint32_t dos_time_hundredths_11004b(void)
{
    uint8_t regs[0x1c];

    regs[1] = 0x2c;
    guest_fn_11f287_x((uint32_t)(uintptr_t)regs, (uint32_t)(uintptr_t)regs);
    return ((((uint32_t)regs[9] * 60u + regs[8]) * 60u + regs[0xd]) * 100u) + regs[0xc];
}
