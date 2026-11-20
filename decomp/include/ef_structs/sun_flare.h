/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SUN_FLARE_H
#define EF_STRUCTS_SUN_FLARE_H

#include "ef_types.h"

/* One "sun_flare" script entry (0xdb4d4), linked at 0x425b20. Field
 * names follow the order of the six "%hd" values; their rendering
 * meaning is not yet known. COUNT signed byte pairs follow at +0x10:
 * points on a circle of RADIUS. */
typedef struct EF_PACKED ef_sun_flare {
    uint8_t colour;                  /* +0x00, value 3: |v| if negative, else v + 0x20 */
    uint8_t count;                   /* +0x01, value 6, at most 10 */
    uint8_t value2;                  /* +0x02, value 2 */
    uint8_t radius;                  /* +0x03, value 1 */
    int16_t value4;                  /* +0x04, value 4 */
    int16_t value5;                  /* +0x06, value 5 */
    int16_t reserved_08;             /* +0x08, cleared */
    uint8_t unknown_0a[2];
    ef_addr32 next;                  /* +0x0c */
} ef_sun_flare;
EF_STATIC_ASSERT(ef_sun_flare_next_off, offsetof(ef_sun_flare, next) == 0xc);
EF_STATIC_ASSERT(ef_sun_flare_size, sizeof(ef_sun_flare) == 0x10);

#endif
