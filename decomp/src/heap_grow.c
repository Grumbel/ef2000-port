/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"
/* ASM exception: obtain the current DS descriptor limit plus one.
 * The native platform must provide this operation explicitly. */
#ifdef __WATCOMC__
static uint32_t growth_segment_end(void);
#pragma aux growth_segment_end = "xor eax,eax" "mov ax,ds" "lsl eax,eax" "inc eax" value [eax] modify [eax]
#else
extern uint32_t growth_segment_end(void);
#endif

/* 0x10d80b, 324 bytes. Extend the break, insert/extend its owning heap,
 * and release the new block so the allocator can retry. Extender services
 * and the alternate allocation path remain explicit external calls. */
uint32_t heap_grow_10d80b(uint32_t size)
{
    uint8_t mode = EF_U8(0x132a5eu);
    uint32_t end, old, length, heapEnd;
    ef_addr32 heap, block;
    if ((mode == 1u && EF_U8(0x132a5fu) == 0u) || mode == 9u)
        return heap_grow_alternate_10d75c(size);
    if (EF_U32(0x17e5dcu) == 0u || EF_U32(0x132a30u) == 0xfffffffeu)
        return 0u;
    if (align_block_size_10d94f(&size) == 0u) return 0u;
    mode = EF_U8(0x132a5eu);
    if (mode >= 2u && mode <= 8u && EF_U8(0x132a60u) == 0u)
        EF_U32(0x132a30u) = growth_segment_end();
    old = EF_U32(0x132a30u);
    end = old + size;
    if (end < old) end = 0xfffffffeu;
    /* Historical symbol name: 0x235 actually adjusts the extender break. */
    old = install_timer_vector_235(end);
    if (old == 0xffffffffu || old > 0xfffffff8u || end <= old)
        return 0u;
    length = end - old;
    size = length - 4u;
    if (size > length) return 0u;
    heap = EF_U32(0x16464cu);
    while (heap != 0u) {
        ef_addr32 next = EF_U32(heap + 8u);
        if (next == 0u || (heap <= old && old < next)) break;
        heap = next;
    }
    if (heap != 0u && old - 4u == heap + EF_U32(heap)) {
        heapEnd = heap + EF_U32(heap);
        size += 4u;
        EF_U32(heap) += size;
        block = heapEnd;
        EF_U32(heapEnd + size) = 0xffffffffu;
    } else {
        if (size < 0x38u) return 0u;
        EF_U32(old) = size;
        heap = old;
        block = insert_heap_block_10d5c7(old);
        size = EF_U32(block);
    }
    EF_U32(block) = size | 1u;
    ++EF_U32(heap + 0x18u);
    EF_U32(heap + 0x14u) = 0xffffffffu;
    heap_free_find_fa9d2(block + 4u);
    return 1u;
}
