/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x35eb8, 75 bytes. EAX=root. Freeze the type byte before either callback;
 * normalize it to zero/one, then apply complementary bit-six settings. */
void net_new_type_35eb8(ef_addr32 root)
{
    uint32_t enabled = EF_U8(0x0013578du) != 0;
    guest_fn_a73cc(root, 0x00183114u, enabled);
    guest_fn_a73cc(root, 0x0018311cu, enabled ^ 1u);
}
