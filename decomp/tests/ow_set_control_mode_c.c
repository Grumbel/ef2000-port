/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static uint32_t set_control_mode_c_original_sequence(void);
#pragma aux set_control_mode_c_original_sequence = \
    0x52 0x8a 0x15 0xc7 0xba 0x15 0x00 0x84 0xd2 0x74 0x02 \
    0x88 0xc2 0x31 0xc0 0x88 0xd0 0x88 0x15 0x05 0xb1 0x12 0x00 \
    0x5a __value [eax] __modify [eax]

uint32_t set_control_mode_c(uint8_t value)
{
    (void)value;
    return set_control_mode_c_original_sequence();
}
