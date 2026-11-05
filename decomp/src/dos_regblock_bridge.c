/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 11f287, 76 bytes: DOS INT 21h bridge using a six-register request block
 * and the X-32 environment. The original preserves request memory except
 * for the six returned register values; it writes 0/-1 for clear/carry in
 * the environment at +0x18 and calls the status adapter at 0x1101e3. */
uint32_t dos_regblock_int21_11f287(ef_addr32 regs, ef_addr32 environment)
{
    uint32_t a = EF_U32(regs);
    uint32_t b = EF_U32(regs + 4u);
    uint32_t c = EF_U32(regs + 8u);
    uint32_t d = EF_U32(regs + 12u);
    uint32_t s = EF_U32(regs + 16u);
    uint32_t i = EF_U32(regs + 20u);
    uint8_t carry = dos_dispatch_int21_11f287(&a, &b, &c, &d, &s, &i);
    uint32_t mask = carry ? 0xffffffffu : 0u;
    EF_U32(regs) = a;
    EF_U32(regs + 4u) = b;
    EF_U32(regs + 8u) = c;
    EF_U32(regs + 12u) = d;
    EF_U32(regs + 16u) = s;
    EF_U32(regs + 20u) = i;
    EF_U32(environment + 0x18u) = mask;
    dos_status_bridge_11f287(EF_U32(environment), mask);
    return EF_U32(environment);
}
