/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Platform boundaries, deliberately unresolved until the extender services
 * are implemented. DPMI takes DI:SI handle words; DOS takes a selector in ES
 * and must restore ES afterward. Neither service result is consumed here. */
extern void ef_heap_release_dpmi(uint16_t handle_low, uint16_t handle_high);
extern void ef_heap_release_dos(uint32_t selector);

/* 0x10d466, 94 bytes plus the shared register-restore tail at 0x10d756.
 * Draft: memory/control flow recovered; guest register/flag equivalence and
 * release-service integration remain pending. */
void heap_release_empty_10d466(void)
{
    ef_addr32 heap = EF_U32(0x16464cu);
    while (heap != 0u) {
        ef_addr32 free_block = EF_U32(heap + 0x24u);
        if (EF_U32(free_block) + 0x2cu == EF_U32(heap)) {
            ef_addr32 backing = heap - 8u;
            ef_addr32 next = EF_U32(heap + 8u);
            uint32_t selector;
            unlink_node_10d427(heap);
            /* Read the backing header after unlinking, as in the original. */
            selector = EF_U32(backing + 4u);
            if (selector == 0u) {
                uint32_t handle = EF_U32(backing);
                ef_heap_release_dpmi((uint16_t)handle, (uint16_t)(handle >> 16));
            } else {
                ef_heap_release_dos(selector);
            }
            /* Release failure does not put the heap back on the list. */
            heap = next;
        } else {
            heap = EF_U32(heap + 8u);
        }
    }
}
