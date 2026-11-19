/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_VIEW_KEYS_H
#define EF_STRUCTS_VIEW_KEYS_H

#include "ef_types.h"

/* 0x19bf80: 20 camera view presets of 0x9c bytes. 0x32c04 compares the
 * key name ("F1".."SF10") at +0x3c; the rest is consumed by 0x2fbc4. */
typedef struct EF_PACKED ef_key_view_preset {
    uint8_t unknown_00[0x3c];
    char key_name[0x60];             /* +0x3c, NUL-terminated */
} ef_key_view_preset;
EF_STATIC_ASSERT(ef_key_view_preset_name_off, offsetof(ef_key_view_preset, key_name) == 0x3c);
EF_STATIC_ASSERT(ef_key_view_preset_size, sizeof(ef_key_view_preset) == 0x9c);

/* 0x19cbb0: 40 secondary view records of 0xa1 bytes, key name at +0x40. */
typedef struct EF_PACKED ef_key_view_track {
    uint8_t unknown_00[0x40];
    char key_name[0x61];             /* +0x40, NUL-terminated */
} ef_key_view_track;
EF_STATIC_ASSERT(ef_key_view_track_name_off, offsetof(ef_key_view_track, key_name) == 0x40);
EF_STATIC_ASSERT(ef_key_view_track_size, sizeof(ef_key_view_track) == 0xa1);

#define EF_KEY_VIEW_PRESET_COUNT 20
#define EF_KEY_VIEW_TRACK_COUNT 40

#endif
