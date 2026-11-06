/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static void set_state_1_106634_original_sequence(void);
#pragma aux set_state_1_106634_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x07 \
    0xc7 0x40 0x04 0x01 0x00 0x00 0x00 \
    __modify [eax]

void EF_CDECL set_state_1_106634(ef_addr32 object)
{
    (void)object;
    set_state_1_106634_original_sequence();
}
