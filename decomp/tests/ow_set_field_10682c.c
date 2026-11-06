/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static void set_field_10682c_original_sequence(void);
#pragma aux set_field_10682c_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x07 \
    0x8b 0x54 0x24 0x08 0x89 0x50 0x30 \
    __modify [eax edx]

void EF_CDECL set_field_10682c(ef_addr32 object, uint32_t value)
{
    (void)object;
    (void)value;
    set_field_10682c_original_sequence();
}
