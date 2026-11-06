/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#if defined(EF_DECOMP_HOST_TEST)
#include "ef_guest_memory.h"
#endif
#include "ef_structs/link_resolution.h"

#if defined(EF_DECOMP_HOST_TEST)
#define LINK_GUEST_MEMORY_PTR(address) ef_guest_memory_ptr(address)
#else
#define LINK_GUEST_MEMORY_PTR(address) ((void *)(uintptr_t)(address))
#endif

ef_addr32 resolve_link_pointer(int16_t index, ef_addr32 candidate)
{
    if (candidate != 0) {
        const ef_link_candidate *entry =
            (const ef_link_candidate *)LINK_GUEST_MEMORY_PTR(candidate);
        if (entry->alternate != 0) { return entry->alternate; }
    }
    return g_link_pointer_table[index];
}

#undef LINK_GUEST_MEMORY_PTR
