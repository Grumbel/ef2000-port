/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"

void sync_flag_12ec8(void)
{
    int32_t index = *(volatile const int32_t *)(uintptr_t)0x0012287eu >> 16;
    volatile uint8_t *record = (volatile uint8_t *)(uintptr_t)
        (0x00121fb4u + (uint32_t)index * 29u);
    volatile uint8_t *mirror = (volatile uint8_t *)(uintptr_t)0x00123790u;
    if (*(volatile const uint8_t *)(uintptr_t)0x00122882u == 0) {
        *mirror = *record;
    } else {
        *mirror = 1;
        *record = 1;
    }
}
