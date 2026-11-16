/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa7d34, 12 bytes. No inputs; EAX scratch, six registers preserved.
 * Sole caller a853a retains ESI. Plain RET. */
void remember_clock_a7d34(void)
{
    EF_U32(0x0041915cu) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
}

/* 0xe1110, 29 bytes. EAX ticks (low word only), six registers preserved,
 * plain RET. Callers e0bb3/e0bfa pass 30/70. */
void wait_ticks_e1110(uint32_t ticks)
{
    uint32_t deadline;
    uint32_t now;

    deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))()
        + (ticks & 0xffffu);
    /* ORIGINAL BUG: plain unsigned deadline comparison across clock wrap.
     * Poll at least once, even for a zero delay; reload callback each time. */
    do {
        now = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
    } while (now < deadline);
}
