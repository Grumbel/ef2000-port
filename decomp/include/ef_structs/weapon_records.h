/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WEAPON_RECORDS_H
#define EF_STRUCTS_WEAPON_RECORDS_H

#include "ef_types.h"

/* 0x12bced: 145-byte weapon records indexed by the selected weapon word
 * 0x1f6f4a. The array is anchored at the two picture names read by
 * 0x69bdc; the true record start may lie before them. */
typedef struct EF_PACKED ef_weapon_record {
    ef_addr32 small_image;           /* +0x00, 320x200 picture name */
    ef_addr32 large_image;           /* +0x04, 640x400 picture name */
    uint8_t unknown_08[0x89];
} ef_weapon_record;
EF_STATIC_ASSERT(ef_weapon_record_large_image_off, offsetof(ef_weapon_record, large_image) == 4);
EF_STATIC_ASSERT(ef_weapon_record_size, sizeof(ef_weapon_record) == 145);

#endif
