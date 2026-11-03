/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUEST_MEMORY_H
#define EF_GUEST_MEMORY_H

#include "ef_types.h"

#if defined(EF_DECOMP_HOST_TEST) && defined(__WATCOMC__)
#error EF_DECOMP_HOST_TEST must never be enabled in an OpenWatcom guest build
#endif

/* In the 32-bit guest, original pointer values address mapped guest RAM.
 * Host-only tests supply a translation for isolated synthetic guest RAM.
 * This hook must never be enabled for an OpenWatcom guest build. */
#if defined(EF_DECOMP_HOST_TEST)
void *ef_guest_memory_ptr(ef_addr32 address);
#else
static void *ef_guest_memory_ptr(ef_addr32 address)
{
    return (void *)(uintptr_t)address;
}
#endif

#endif
