/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static uint32_t exchange_slot_original_sequence(void);
/* The original ABI intentionally clobbers EBX with the new value. */
#pragma aux exchange_slot_original_sequence = \
    0x8b 0x44 0x24 0x04 0x8b 0x5c 0x24 0x08 \
    0x8b 0x14 0x85 0xcc 0xd7 0x4d 0x00 \
    0x89 0x1c 0x85 0xcc 0xd7 0x4d 0x00 0x89 0xd0 \
    __value [eax] __modify [eax edx]

uint32_t EF_CDECL exchange_slot_1041e4(uint32_t index, uint32_t value)
{
    (void)index;
    (void)value;
    return exchange_slot_original_sequence();
}
