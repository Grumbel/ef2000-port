/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"

EF_STATIC_ASSERT(hiscore_entry_size, sizeof(ef_hiscore_entry) == 25);
EF_STATIC_ASSERT(hiscore_score_offset, offsetof(ef_hiscore_entry, score) == 21);
EF_STATIC_ASSERT(hiscore_table_size, sizeof(g_hiscore_table) == 250);
EF_STATIC_ASSERT(hiscore_last_score_offset,
                 9 * sizeof(ef_hiscore_entry) + offsetof(ef_hiscore_entry, score) == 246);

/* 0x000432c  size 0x0f  C candidate: not yet compiled/spliced/replay-verified.
 * Both scores are signed int32_t. The callers use only AL, not all of EAX.
 * A tie does NOT qualify, unlike the separate insertion function. */
uint8_t hiscore_qualifies(void)
{
    return (uint8_t)(g_hiscore_current > g_hiscore_table[9].score);
}
