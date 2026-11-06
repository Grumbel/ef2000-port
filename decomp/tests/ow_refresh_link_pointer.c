/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#if defined(EF_DECOMP_HOST_TEST)
#include "ef_guest_memory.h"
#endif
#include "ef_structs/link_resolution.h"

#if defined(EF_DECOMP_HOST_TEST)
#define LINK_GUEST_MEMORY_PTR(address) ef_guest_memory_ptr(address)
#else
#define LINK_GUEST_MEMORY_PTR(address) ((void *)(uintptr_t)(address))
#endif

ef_addr32 refresh_link_pointer(ef_addr32 reference)
{
    ef_link_reference *record =
        (ef_link_reference *)LINK_GUEST_MEMORY_PTR(reference);
    ef_addr32 found = resolve_link_pointer(record->index, record->current);
    record->current = found;
    return found;
}

#undef LINK_GUEST_MEMORY_PTR
