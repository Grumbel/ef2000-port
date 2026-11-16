/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xe994c, 106 bytes. EAX flags, EDX/EBX/ECX angles; plain RET.
 * ECX/ESI/EDI/EBP preserved, EAX/EDX/EBX scratch. Callers 80b95
 * and 83a66 reload vector components before their transform calls. */
void build_rotation_e994c(uint32_t flags, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t first, second;

    if ((flags & 1u) != 0u)
        guest_fn_e95c0(a, b, c);
    else
        guest_fn_e977c(a, b, c);
    if ((flags & 2u) == 0u)
        return;

    first = EF_U32(0x0046444cu);
    second = EF_U32(0x00464444u);
    EF_U32(0x00464444u) = first;
    EF_U32(0x0046444cu) = second;
    first = EF_U32(0x00464458u);
    second = EF_U32(0x00464448u);
    EF_U32(0x00464448u) = first;
    EF_U32(0x00464458u) = second;
    first = EF_U32(0x0046445cu);
    second = EF_U32(0x00464454u);
    EF_U32(0x00464454u) = first;
    EF_U32(0x0046445cu) = second;
}
