/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Leaf helpers from the unknown-function tranche (Stream B, tranche 1). */

/* 0xdc5a9, 158 bytes. Three caller-stack arguments (ret 0xc): X, Y, DST.
 * S = (X + Y) * 0x12345678 (wrapping). The divisor is S >> 16 (arithmetic),
 * raised to at least 0x20. The quotient is (S >> 8) / divisor, clamped to
 * +-0x3e80. The word quotient + 0x1234 is stored at DST and again at DST+2,
 * as both original stos instructions compute the same value. */
#pragma aux pair_word_dc5a9 parm routine [] modify [eax ecx edx]
void pair_word_dc5a9(int32_t x, int32_t y, ef_addr32 dst)
{
    uint32_t sum = (uint32_t)x * 0x12345678u + (uint32_t)y * 0x12345678u;
    int32_t divisor = (int32_t)sum >> 16;
    int32_t quotient;
    uint16_t word;

    if (divisor <= 0x20)
        divisor = 0x20;
    quotient = ((int32_t)sum >> 8) / divisor;
    if (quotient >= 0x3e80)
        quotient = 0x3e80;
    if (quotient <= -0x3e80)
        quotient = -0x3e80;
    word = (uint16_t)(quotient + 0x1234);
    EF_U16(dst) = word;
    EF_U16(dst + 2u) = word;
}
