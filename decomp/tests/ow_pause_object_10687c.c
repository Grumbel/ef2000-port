/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
static void pause_object_10687c_original_sequence(void);
#pragma aux pause_object_10687c_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x0d \
    0x83 0x78 0x04 0x04 0x75 0x07 \
    0xc7 0x40 0x04 0x08 0x00 0x00 0x00 \
    __modify [eax]

void EF_CDECL pause_object_10687c(ef_addr32 object)
{
    (void)object;
    pause_object_10687c_original_sequence();
}
