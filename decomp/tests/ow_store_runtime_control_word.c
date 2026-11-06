/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

#pragma aux store_runtime_control_word_direct = \
    0xa3 0xbc 0x35 0x1a 0x00 __modify []
static void store_runtime_control_word_direct(void);

void store_runtime_control_word(uint32_t value)
{
    (void)value;
    store_runtime_control_word_direct();
}
