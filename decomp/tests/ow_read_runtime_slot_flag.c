/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#pragma pack(push, 1)
struct unaligned_u32 { uint32_t value; };
#pragma pack(pop)
uint8_t read_runtime_slot_flag(int16_t subindex, int16_t group)
{
    uint32_t packed = ((const struct unaligned_u32 *)(uintptr_t)
                       (0x00121ec6u + (uint32_t)(int32_t)group * 2u))->value;
    int32_t base = (int32_t)packed >> 16;
    int32_t index = base + subindex;
    return *(const volatile uint8_t *)(uintptr_t)
           (0x00121fb4u + (uint32_t)index * 29u);
}
