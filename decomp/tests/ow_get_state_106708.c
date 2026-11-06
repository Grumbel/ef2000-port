/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

uint32_t EF_CDECL get_state_106708(ef_addr32 object)
{
    if (object == 0)
        return 0;
    return *(uint32_t *)(uintptr_t)(object + 4u);
}
