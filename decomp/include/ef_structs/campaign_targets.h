/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_CAMPAIGN_TARGETS_H
#define EF_STRUCTS_CAMPAIGN_TARGETS_H

#include "ef_types.h"

/* 0x197370: group and member of each numbered campaign target button. */
typedef struct EF_PACKED ef_campaign_target_choice {
    int16_t group;                   /* +0 */
    int16_t member;                  /* +2 */
} ef_campaign_target_choice;
EF_STATIC_ASSERT(ef_campaign_target_choice_size, sizeof(ef_campaign_target_choice) == 4);

/* 0x197500: saved map marker positions, 12 bytes per marker. */
typedef struct EF_PACKED ef_saved_marker_position {
    uint32_t x;                      /* +0, marker +0 */
    uint32_t y;                      /* +4, marker +4 (flipped) */
    uint8_t unknown_08[4];
} ef_saved_marker_position;
EF_STATIC_ASSERT(ef_saved_marker_position_size, sizeof(ef_saved_marker_position) == 12);

#endif
