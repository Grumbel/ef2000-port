/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_MAP_ENTRY_H
#define EF_STRUCTS_MAP_ENTRY_H

#include "ef_types.h"

#define EF_MAP_ENTRY_TABLE 0x00211af0u
#define EF_MAP_ENTRY_COUNT 0x28

/* The 48-byte map entries at 0x211af0 (0x573d0, 0x90ac4, 0x90f50,
 * 0x910f4). Only observed offsets have names. */
typedef struct EF_PACKED ef_map_entry {
    int32_t x;                        /* +0x00 */
    int32_t z;                        /* +0x04 */
    int32_t y;                        /* +0x08 */
    int16_t words[3];                 /* +0x0c */
    uint32_t clock;                   /* +0x12 */
    int16_t index;                    /* +0x16, own slot number */
    uint16_t marker;                  /* +0x18, g_world_grid_marker */
    uint8_t unknown_1a[6];
    int16_t kind;                     /* +0x20 */
    uint16_t score;                   /* +0x22 */
    uint8_t flags;                    /* +0x24, bits 0..2 state */
    uint8_t unknown_25;
    int32_t jitter[2];                /* +0x26, -200..199 */
    int16_t id;                       /* +0x2e */
} ef_map_entry;

EF_STATIC_ASSERT(ef_map_entry_size, sizeof(ef_map_entry) == 0x30);
EF_STATIC_ASSERT(ef_map_entry_clock_off, offsetof(ef_map_entry, clock) == 0x12);
EF_STATIC_ASSERT(ef_map_entry_kind_off, offsetof(ef_map_entry, kind) == 0x20);
EF_STATIC_ASSERT(ef_map_entry_flags_off, offsetof(ef_map_entry, flags) == 0x24);
EF_STATIC_ASSERT(ef_map_entry_id_off, offsetof(ef_map_entry, id) == 0x2e);

#endif
