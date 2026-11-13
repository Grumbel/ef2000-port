/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x120485, 49 bytes. Install once, but apply the x87 control word on
 * every call, including the already-installed path. */
void install_runtime_hook_120485(void)
{
    if (EF_U32(0x00160790u) != 0x001207a3u) {
        EF_U32(0x00132a69u) = 0x0010dc4bu;
        guest_fn_120746_x();
        EF_U32(0x00160790u) = 0x001207a3u;
    }
    guest_fn_120800_x();
}
