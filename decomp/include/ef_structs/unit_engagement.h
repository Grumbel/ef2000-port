/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_STRUCTS_UNIT_ENGAGEMENT_H
#define EF_STRUCTS_UNIT_ENGAGEMENT_H
#include "ef_types.h"

/* Prefix read by the unit/target distance gate at 0x29f54. */
typedef struct EF_PACKED ef_engagement_unit {
    int32_t x_fixed;
    uint8_t unknown_04[4];
    int32_t z_fixed;
    uint8_t unknown_0c[12];
    uint8_t category;
    uint8_t unknown_19[3];
    ef_addr32 target;
    uint8_t unknown_20[6];
    int32_t grid_packed;
} ef_engagement_unit;
EF_STATIC_ASSERT(ef_engagement_x_off, offsetof(ef_engagement_unit, x_fixed) == 0);
EF_STATIC_ASSERT(ef_engagement_z_off, offsetof(ef_engagement_unit, z_fixed) == 8);
EF_STATIC_ASSERT(ef_engagement_category_off, offsetof(ef_engagement_unit, category) == 0x18);
EF_STATIC_ASSERT(ef_engagement_target_off, offsetof(ef_engagement_unit, target) == 0x1c);
EF_STATIC_ASSERT(ef_engagement_grid_off, offsetof(ef_engagement_unit, grid_packed) == 0x26);
EF_STATIC_ASSERT(ef_engagement_size, sizeof(ef_engagement_unit) == 0x2a);
#endif
