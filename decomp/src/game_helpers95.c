/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Bearing between two 3-word positions. */

/* 0xdcaa4, 207 bytes. Pitch and yaw angles of the vector from position b to position a, stored as two words at out. */
void bearing_between_positions_dcaa4(ef_addr32 out, ef_addr32 a, ef_addr32 b)
{
    int32_t d[3];
    int32_t r;

    d[0] = (int32_t)EF_S16(a) - (int32_t)EF_S16(b);
    d[1] = (int32_t)EF_S16(a + 2u) - (int32_t)EF_S16(b + 2u);
    d[2] = (int32_t)EF_S16(a + 4u) - (int32_t)EF_S16(b + 4u);
    if (d[0] < -0x8000 || d[0] > 0x7fff || d[1] < -0x8000 || d[1] > 0x7fff || d[2] < -0x8000 || d[2] > 0x7fff)
        halve_triple_to_fit16_dc890((uint32_t)(uintptr_t)&d[0], (uint32_t)(uintptr_t)&d[1], (uint32_t)(uintptr_t)&d[2]);
    r = (int32_t)length_2d_decb0(d[0], d[2]);
    EF_U16(out) = (uint16_t)angle_of_b3b1c(d[1], r);
    EF_U16(out + 2u) = (uint16_t)angle_of_b3b1c((int32_t)EF_S16(a) - (int32_t)EF_S16(b),
                                                 (int32_t)EF_S16(a + 4u) - (int32_t)EF_S16(b + 4u));
}
