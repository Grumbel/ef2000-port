/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_heap_services.h"

/* 0x10d75c, 175 bytes plus the shared epilogue at 0x10d753.
 * Draft: alternate extender services and exact guest register/flag ABI pending. */
uint32_t heap_grow_alternate_10d75c(uint32_t size)
{
    ef_addr32 heap, block;
    uint32_t length;
    if (EF_U32(0x17e5dcu) == 0u || EF_U32(0x132a30u) == 0xfffffffeu)
        return 0u;
    if (align_block_size_10d94f(&size) == 0u) return 0u;
    if (EF_U8(0x132a5eu) == 1u) {
        heap = heap_alloc_mode1_10d689(size);
        if (heap != 0u) size = EF_U32(heap);
    } else {
        ef_heap_extended_result result;
        ef_heap_allocate_extended(size, &result);
        heap = result.carry == 0u ? result.address : 0u;
        size -= 4u;
    }
    if (heap == 0u) return 0u;
    length = size - 4u;
    if (length > size || length < 0x38u) return 0u;
    EF_U32(heap) = length;
    block = insert_heap_block_10d5c7(heap);
    EF_U32(block) |= 1u;
    EF_U32(heap + 0x14u) = 0xffffffffu;
    ++EF_U32(heap + 0x18u);
    heap_free_find_fa9d2(block + 4u);
    return 1u;
}
