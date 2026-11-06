/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
static void encoder_timing_nop(void);
#pragma aux encoder_timing_nop = 0x90 __modify []
#pragma pack(push, 1)
struct unaligned_u32 { uint32_t value; };
#pragma pack(pop)
uint32_t encode_runtime_segment_offset(ef_addr32 pointer)
{
    uint32_t base = ((const struct unaligned_u32 *)
                     (uintptr_t)0x00120cc6u)->value;
    uint32_t delta = pointer - base;
    uint32_t encoded = delta & 0x0fu;
    encoded += ((delta >> 4) & 0xffffu) << 16;
    encoder_timing_nop();
    encoder_timing_nop();
    return encoded;
}
