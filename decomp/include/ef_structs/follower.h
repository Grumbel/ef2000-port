/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_FOLLOWER_H
#define EF_STRUCTS_FOLLOWER_H

#include "ef_types.h"

/* The 69-byte follower rows at g_external_grid_base (0x37eac, 0x381a0).
 * Only observed offsets have names. */
typedef struct EF_PACKED ef_follower_record {
    uint8_t state;                    /* +0x00, 1 while following */
    uint8_t unknown_01[6];
    int32_t position[3];              /* +0x07, scene position >> 8 */
    uint8_t active;                   /* +0x13 */
    uint8_t unknown_14[8];
    uint8_t level;                    /* +0x1c */
    uint8_t unknown_1d[0x27];
    uint8_t marker;                   /* +0x44, matches g_world_grid_marker */
} ef_follower_record;

EF_STATIC_ASSERT(ef_follower_size, sizeof(ef_follower_record) == 0x45);
EF_STATIC_ASSERT(ef_follower_active_off, offsetof(ef_follower_record, active) == 0x13);
EF_STATIC_ASSERT(ef_follower_level_off, offsetof(ef_follower_record, level) == 0x1c);
EF_STATIC_ASSERT(ef_follower_marker_off, offsetof(ef_follower_record, marker) == 0x44);

#endif
