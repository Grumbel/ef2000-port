/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SUN_FLARE_H
#define EF_STRUCTS_SUN_FLARE_H

#include "ef_types.h"

/* One "sun_flare" (0xdb4d4) or "tmsun_flare" (0xdb654) script entry,
 * linked at 0x425b20. Field names follow the order of the "%hd" values;
 * their rendering meaning is not yet known. A sun_flare entry is
 * followed at +0x10 by COUNT signed byte pairs: points on a circle of
 * RADIUS. A tmsun_flare entry has no points; it names a sun texture. */
typedef struct EF_PACKED ef_sun_flare {
    uint8_t colour;                  /* +0x00, sun_flare value 3: |v| if negative, else v + 0x20 */
    uint8_t count;                   /* +0x01, sun_flare value 6, at most 10 */
    uint8_t value2;                  /* +0x02, value 2 */
    uint8_t radius;                  /* +0x03, value 1 */
    int16_t value4;                  /* +0x04, value 4 */
    int16_t value5;                  /* +0x06, value 5 */
    int16_t textured;                /* +0x08, 0 for sun_flare, 1 for tmsun_flare */
    int16_t texture;                 /* +0x0a, tmsun_flare value 3, below g_sun_texture_count */
    ef_addr32 next;                  /* +0x0c */
} ef_sun_flare;
EF_STATIC_ASSERT(ef_sun_flare_next_off, offsetof(ef_sun_flare, next) == 0xc);
EF_STATIC_ASSERT(ef_sun_flare_size, sizeof(ef_sun_flare) == 0x10);

#endif
