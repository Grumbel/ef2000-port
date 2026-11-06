/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static ef_addr32 find_last_char_original_sequence(void);
#pragma aux find_last_char_original_sequence = \
    0x53 0x51 0x88 0xd1 0x31 0xdb \
    0x3a 0x08 0x75 0x02 0x89 0xc3 \
    0x89 0xc2 0x8a 0x2a 0x40 0x84 0xed 0x75 0xf1 \
    0x89 0xd8 0x59 0x5b \
    __value [eax] __modify [edx]

ef_addr32 find_last_char(ef_addr32 text, uint8_t ch)
{
    (void)text;
    (void)ch;
    return find_last_char_original_sequence();
}
