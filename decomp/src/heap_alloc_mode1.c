/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_heap_services.h"

/* 0x10d689, 211 bytes. Mode-1 backing allocation, with cleanup first,
 * DPMI primary allocation and selector-based DOS fallback.
 * Draft: platform services and exact guest register/flag ABI pending. */
ef_addr32 heap_alloc_mode1_10d689(uint32_t size)
{
    ef_heap_dpmi_result result;
    ef_addr32 address = 0u;
    uint32_t reservation, selector;
    heap_release_empty_10d466();
    ef_heap_allocate_dpmi(size, &result);
    if (result.carry == 0u) {
        address = result.address;
        /* These stores precede the null-address test in the original. */
        EF_U16(address) = result.handle_low;
        EF_U16(address + 2u) = result.handle_high;
    }
    if (address != 0u) {
        EF_U32(address + 8u) = size - 8u;
        EF_U32(address + 4u) = 0u;
        return address + 8u;
    }
    if ((EF_U16(0x4ddeeau) & 0xfff0u) != 0u)
        EF_U32(0x4ddee8u) = 0xfffffu;
    if (size > 0x10000u) return 0u;
    reservation = ef_heap_allocate_dos((EF_U32(0x4ddee8u) >> 4) | 1u);
    if ((reservation & 0x80000000u) != 0u) return 0u;
    selector = ef_heap_allocate_dos(size >> 4);
    /* Always release the temporary allocation before testing the second. */
    ef_heap_release_dos(reservation);
    if ((selector & 0x80000000u) != 0u) return 0u;
    address = ef_heap_selector_base(selector) + 8u;
    EF_U32(address - 4u) = selector;
    EF_U32(address) = size - 8u;
    return address;
}
