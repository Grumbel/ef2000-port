/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_types.h"

EF_STATIC_ASSERT(ef_u32_is_4, sizeof(uint32_t) == 4);

uint32_t ef_omf_fixture(uint32_t value)
{
    return value + 1U;
}
