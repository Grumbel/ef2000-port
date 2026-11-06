/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

uint32_t field_10dfe0(void)
{
    return *(uint32_t *)(uintptr_t)(g_runtime_segment_base + 0x46cu);
}
