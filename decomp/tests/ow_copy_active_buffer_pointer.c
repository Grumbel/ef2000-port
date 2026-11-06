/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static uint32_t copy_active_buffer_pointer_direct(void);
#pragma aux copy_active_buffer_pointer_direct = \
    0xa1 0xc4 0xaf 0x12 0x00 0xa3 0x9c 0x8e 0x1f 0x00 \
    __value [eax] __modify [eax]

uint32_t copy_active_buffer_pointer(void)
{
    return copy_active_buffer_pointer_direct();
}
