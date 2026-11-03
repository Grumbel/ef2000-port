/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_HISCORE_H
#define EF_STRUCTS_HISCORE_H

#include "ef_types.h"

/* Guest 0x1970a4: ten records, 25 bytes each. The signed 32-bit score is
 * stored at the unaligned offset +21; do not rely on host alignment. */
typedef struct EF_PACKED ef_hiscore_entry {
    char name[20];
    uint8_t difficulty;
    int32_t score;
} ef_hiscore_entry;

#endif
