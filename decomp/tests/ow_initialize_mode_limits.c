/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

#pragma aux mode_limit_timing_nop = 0x90 __modify []
static void mode_limit_timing_nop(void);
#pragma aux write_first_mode_limit = \
    0x66 0xc7 0x05 0xe2 0x6c 0x1a 0x00 0x08 0x00 __modify []
static void write_first_mode_limit(void);
#pragma aux write_second_mode_limit = \
    0x66 0xc7 0x05 0xe8 0x6c 0x1a 0x00 0x0a 0x00 __modify []
static void write_second_mode_limit(void);

void initialize_mode_limits(void)
{
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    write_first_mode_limit();
    write_second_mode_limit();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
}
