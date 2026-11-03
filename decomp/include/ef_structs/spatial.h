/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SPATIAL_H
#define EF_STRUCTS_SPATIAL_H

#include "ef_types.h"

/* Provisional names: each 0x263ec entry stores AX, DX at offsets 0, 2. */
typedef struct {
    int16_t x;
    int16_t y;
} ef_spatial_slot;

EF_STATIC_ASSERT(spatial_slot_size, sizeof(ef_spatial_slot) == 4);
EF_STATIC_ASSERT(spatial_slot_y_offset, offsetof(ef_spatial_slot, y) == 2);

#endif
