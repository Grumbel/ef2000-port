/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static uint32_t exchange_callback_original_sequence(void);
/* The original stack ABI intentionally clobbers EBX on a non-null object. */
#pragma aux exchange_callback_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x75 0x01 0xc3 \
    0x8b 0x5c 0x24 0x08 \
    0x8b 0x90 0x50 0x08 0x00 0x00 \
    0x89 0x98 0x50 0x08 0x00 0x00 0x89 0xd0 \
    __value [eax] __modify [edx]

uint32_t EF_CDECL exchange_callback_106938(ef_addr32 object, uint32_t value)
{
    (void)object;
    (void)value;
    return exchange_callback_original_sequence();
}
