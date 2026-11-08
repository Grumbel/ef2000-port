/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GUEST_MEMORY_H
#define EF_GUEST_MEMORY_H

#include "ef_types.h"

#if (defined(EF_DECOMP_HOST_TEST) || defined(EF_DECOMP_NATIVE)) && defined(__WATCOMC__)
#error Host guest-memory translation must never be enabled in an OpenWatcom guest build
#endif

/* In the 32-bit guest, original pointer values address mapped guest RAM.
 * Host tests and the native Linux decomp provide a bounded guest-RAM mapping.
 * OpenWatcom uses direct addresses inside its original 32-bit address space. */
#if defined(EF_DECOMP_HOST_TEST) || defined(EF_DECOMP_NATIVE)
void *ef_guest_memory_ptr(ef_addr32 address);
#else
static void *ef_guest_memory_ptr(ef_addr32 address)
{
    return (void *)(uintptr_t)address;
}
#endif

#endif
