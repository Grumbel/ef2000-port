/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* Segment-register access cannot be expressed in ISO C. This adapter only
 * reads DS; the allocation/search/retry algorithm below is recovered C.
 * Native bindings must supply the guest selector, not a host segment. */
#ifdef __WATCOMC__
static uint32_t heap_data_selector(void);
#pragma aux heap_data_selector = "xor eax,eax" "mov ax,ds" value [eax] modify [eax]
#else
extern uint32_t heap_data_selector(void);
#endif

/* 0xfc795, 227 bytes. Search the cached heap, track the largest failed
 * block, and retry after extender growth. No host allocator substitution. */
ef_addr32 allocate_heap_fc795(uint32_t size)
{
    uint32_t aligned, threshold, grew = 0u;
    ef_addr32 heap, result = 0u;
    if (size == 0u || size > 0xffffffd4u)
        return 0u;
    aligned = (size + 3u) & 0xfffffffcu;
    for (;;) {
        threshold = aligned < 12u ? 12u : aligned;
        heap = 0u;
        if (threshold > EF_U32(0x164654u))
            heap = EF_U32(0x164650u);
        if (heap == 0u) {
            EF_U32(0x164654u) = 0u;
            heap = EF_U32(0x16464cu);
        }
        while (heap != 0u) {
            EF_U32(0x164650u) = heap;
            result = heap_alloc_110410(size, heap_data_selector(), heap);
            if (result != 0u)
                goto finished;
            if (EF_U32(heap + 0x14u) > EF_U32(0x164654u))
                EF_U32(0x164654u) = EF_U32(heap + 0x14u);
            heap = EF_U32(heap + 8u);
        }
        if (grew == 0u && heap_grow_10d80b(size) != 0u) {
            grew = 1u;
            continue;
        }
        if (return_zero_1105c3() == 0u)
            break;
        grew = 0u;
    }
finished:
    EF_U8(0x4dd8a1u) = 0u;
    return result;
}
