/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

#pragma aux store_unit_counter_zero = \
    0x66 0xc7 0x05 0x4e 0x81 0x12 0x00 0x00 0x00 __modify []
static void store_unit_counter_zero(void);
void reset_unit_counter(void)
{
    store_unit_counter_zero();
}
