/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

#pragma aux write_runtime_phase_five = \
    0x66 0xc7 0x05 0xec 0x35 0x1a 0x00 0x05 0x00 __modify []
static void write_runtime_phase_five(void);
void set_runtime_phase_five(void)
{
    write_runtime_phase_five();
}
