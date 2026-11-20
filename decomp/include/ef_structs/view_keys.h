/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_VIEW_KEYS_H
#define EF_STRUCTS_VIEW_KEYS_H

#include "ef_types.h"

#define EF_KEY_VIEW_STEPS 10

/* 0x19bf80: 20 camera view presets of 0x9c bytes. 0x32c04 compares the
 * key name ("F1".."SF10") at +0x3c; 0x2fbc4 consumes the view itself and
 * 0x31514 steps through the scripted tracks. */
typedef struct EF_PACKED ef_key_view_preset {
    uint8_t unknown_00[0x3c];
    char key_name[0x0f];             /* +0x3c, NUL-terminated */
    uint8_t repeat;                  /* +0x4b, 1: restart the preset after the last step */
    int16_t track[EF_KEY_VIEW_STEPS]; /* +0x4c, track per step, -1 ends */
    int16_t hold[EF_KEY_VIEW_STEPS];  /* +0x60, frames per step */
    int32_t range[EF_KEY_VIEW_STEPS];     /* +0x74, 0 or the eye distance limit */
} ef_key_view_preset;
EF_STATIC_ASSERT(ef_key_view_preset_name_off, offsetof(ef_key_view_preset, key_name) == 0x3c);
EF_STATIC_ASSERT(ef_key_view_preset_track_off, offsetof(ef_key_view_preset, track) == 0x4c);
EF_STATIC_ASSERT(ef_key_view_preset_hold_off, offsetof(ef_key_view_preset, hold) == 0x60);
EF_STATIC_ASSERT(ef_key_view_preset_range_off, offsetof(ef_key_view_preset, range) == 0x74);
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
