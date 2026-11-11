/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdint.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* AIL IRQ table initializer; kept out of misc_drafts1 because it calls the
 * host-boundary services (guest_ail_irq_*), which the hybrid linker cannot
 * resolve and which would otherwise block the whole module. */

/* 0x110fd4, 241 bytes. Full cold AIL IRQ table initializer through its
 * original return at 0x1110c4. The actual pushf/CLI/ES operations are represented
 * by a strict host boundary that supplies the already captured DS selector;
 * this never disables interrupts on Linux. The original 0x111575 lock helper
 * and the four 16-dword table clears are preserved in their original order.
 * DOS queries and vector installation use opt-in adapters that reject real
 * interrupts. The recovered timer callback and PIT writes also run only
 * behind inert, opt-in host services. */
void initialize_ail_irq_tables_110fd4(void)
{
    uint32_t i;
    uint64_t original_protected_vector;
    uint32_t original_realmode_vector;
    EF_U16(0x0017d7beu) = guest_ail_irq_enter_110fd9();
    register_code_ranges_111575();
    EF_U32(0x0017d7b6u) = 0u;
    EF_U32(0x0017d7bau) = 0u;
    EF_U32(0x0017d7b2u) = UINT32_MAX;
    EF_U32(0x0017d7c0u) = UINT32_MAX;
    for (i = 0u; i < 16u; i++)
        EF_U32(0x0017d664u + i * 4u) = 0u;
    for (i = 0u; i < 16u; i++)
        EF_U32(0x0017d6a4u + i * 4u) = 0u;
    for (i = 0u; i < 16u; i++)
        EF_U32(0x0017d6e4u + i * 4u) = 0u;
    for (i = 0u; i < 16u; i++)
        EF_U32(0x0017d724u + i * 4u) = 0u;
    /* Original AX=2502h returns protected ES:EBX, then AX=2503h returns
     * the real-mode EBX. Preserve both values in their original slots. */
    original_protected_vector = guest_ail_irq8_vector_query_11105b(8u);
    original_realmode_vector = guest_ail_irq8_realmode_query_111065(8u);
    EF_U32(0x0017d7a4u) = (uint32_t)original_protected_vector;
    EF_U16(0x0017d7a8u) = (uint16_t)(original_protected_vector >> 32u);
    EF_U32(0x0017d7aau) = original_realmode_vector;
    /* AX=2506h receives CL=8, EDX=original 0x110de6 handler, DS=CS. */
    guest_ail_irq8_install_111097(8u, 0x00110de6u,
                                 guest_ail_code_selector_111089());
    EF_U32(0x0017d6a0u) = 2u;
    /* 0x1021f4 wraps original 0x1114c7, 0x110f6f and 0x110f0c. */
    guest_ail_timer_setup_1110ab(0x3cu, 0xd68du);
    /* Original 0x1110b3..0x1110c4 restores interrupt flags and registers. */
    guest_ail_irq_exit_1110b3();
}

