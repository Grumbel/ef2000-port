/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xe99b8, 106 bytes. EAX destination, EDX/EBX/ECX components.
 * EDX/EBX/ECX scratch; ESI/EDI/EBP preserved, plain RET.
 * Keep coefficient loads around stores: destination may alias the matrix. */
void transform_vector_e99b8(ef_addr32 out, int32_t x, int32_t y, int32_t z)
{
    uint32_t a = (uint32_t)x, b = (uint32_t)y, c = (uint32_t)z;
    uint32_t first, second, last_b, last_c, term;

    first = EF_U32(0x0046444cu) * b;
    term = EF_U32(0x00464440u) * a;
    first += term;
    term = EF_U32(0x00464458u) * c;
    first += term;
    second = EF_U32(0x00464444u) * a;
    EF_U32(out) = first * 2u;

    term = EF_U32(0x00464450u) * b;
    second += term;
    term = EF_U32(0x0046445cu) * c;
    last_c = EF_U32(0x00464460u) * c;
    second += term;
    last_b = EF_U32(0x00464454u) * b;
    EF_U32(out + 4u) = second * 2u;

    term = EF_U32(0x00464448u) * a;
    term += last_b;
    term += last_c;
    EF_U32(out + 8u) = term * 2u;
}
