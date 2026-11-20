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

/* One (name, handler) pair of a keyword table: the READCOLS keywords at
 * 0x15bb80 and the "game_type" values at 0x15bd40. */
typedef struct EF_PACKED ef_env_keyword {
    ef_addr32 name;
    ef_addr32 handler;
} ef_env_keyword;
EF_STATIC_ASSERT(ef_env_keyword_size, sizeof(ef_env_keyword) == 8);

/* One "ref_point" script line (0xdac2c). 256 of them at 0x421a48. */
typedef struct EF_PACKED ef_ref_point {
    int16_t x;                       /* +0x00 */
    int16_t y;                       /* +0x02 */
    ef_addr32 name;                  /* +0x04, uppercased heap copy */
    uint8_t kind;                    /* +0x08, 1 or 2, else 0 */
} ef_ref_point;
EF_STATIC_ASSERT(ef_ref_point_size, sizeof(ef_ref_point) == 9);

#define EF_CRATER_TYPES 43

/* One 110-byte crater record of g_crater_records or g_crater_slots
 * ("set_nb_craters"). "load_craters" (0xdaef4) fills one shape per
 * crater type and links the records into a ring. */
typedef struct EF_PACKED ef_crater_record {
    uint8_t unknown_00[0x12];
    int16_t state;                   /* +0x12, set to -1 by load_craters */
    int16_t shapes[EF_CRATER_TYPES]; /* +0x14, per crater type */
    ef_addr32 next;                  /* +0x6a, ring link */
} ef_crater_record;
EF_STATIC_ASSERT(ef_crater_record_next_off, offsetof(ef_crater_record, next) == 0x6a);
EF_STATIC_ASSERT(ef_crater_record_size, sizeof(ef_crater_record) == 110);

#endif
