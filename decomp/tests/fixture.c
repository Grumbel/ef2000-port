/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_types.h"
#include "ef_structs/hiscore.h"

EF_STATIC_ASSERT(ef_u32_is_4, sizeof(uint32_t) == 4);
EF_STATIC_ASSERT(ef_hiscore_size, sizeof(ef_hiscore_entry) == 25);
EF_STATIC_ASSERT(ef_hiscore_name_off, offsetof(ef_hiscore_entry, name) == 0);
EF_STATIC_ASSERT(ef_hiscore_difficulty_off, offsetof(ef_hiscore_entry, difficulty) == 20);
EF_STATIC_ASSERT(ef_hiscore_score_off, offsetof(ef_hiscore_entry, score) == 21);

uint32_t ef_omf_fixture(uint32_t value)
{
    return value + 1U;
}
