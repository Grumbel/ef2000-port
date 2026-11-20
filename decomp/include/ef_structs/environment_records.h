/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_ENVIRONMENT_RECORDS_H
#define EF_STRUCTS_ENVIRONMENT_RECORDS_H

#include "ef_types.h"

/* One "lightning_point" script line (0xdad4c): four "%ld" values
 * scaled by 2048 and an "%i" value. Twenty of them at 0x4218e0. */
typedef struct EF_PACKED ef_lightning_area {
    int32_t bounds[4];               /* +0x00, script values 1-4 << 11 */
    int16_t value;                   /* +0x10, script value 5 */
} ef_lightning_area;
EF_STATIC_ASSERT(ef_lightning_area_size, sizeof(ef_lightning_area) == 18);

/* One "cloud_fade" script line (0xdb730): four "%d" values kept whole
 * and two kept as bytes. Two of them at 0x425b64. */
typedef struct EF_PACKED ef_cloud_level {
    int32_t values[4];               /* +0x00, script values 1-4 */
    uint8_t bytes[2];                /* +0x10, script values 5-6 */
} ef_cloud_level;
EF_STATIC_ASSERT(ef_cloud_level_size, sizeof(ef_cloud_level) == 18);

/* One "heavenly_body" script line (0xdb8bc), indexed by its first
 * value. Forty of them at 0x422398. */
typedef struct EF_PACKED ef_heavenly_body {
    int16_t angle;                   /* +0x00, negated "%hd" value */
    uint8_t horizon_fixed;           /* +0x02, g_horizon_fixed when read */
    int16_t shape;                   /* +0x03, AX of the name lookup 0x9ddfc */
} ef_heavenly_body;
EF_STATIC_ASSERT(ef_heavenly_body_size, sizeof(ef_heavenly_body) == 5);

#endif
