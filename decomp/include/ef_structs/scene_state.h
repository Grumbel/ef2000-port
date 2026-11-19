/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SCENE_STATE_H
#define EF_STRUCTS_SCENE_STATE_H

#include "ef_types.h"

/* 0x0c4756: three packed 32-bit values separated by unknown 2-byte gaps. */
typedef struct EF_PACKED ef_scene_triplet_packed {
    int32_t x;
    uint8_t pad_04[2];
    int32_t y;
    uint8_t pad_0a[2];
    int32_t z;
} ef_scene_triplet_packed;
EF_STATIC_ASSERT(ef_scene_triplet_x, offsetof(ef_scene_triplet_packed, x) == 0);
EF_STATIC_ASSERT(ef_scene_triplet_y, offsetof(ef_scene_triplet_packed, y) == 6);
EF_STATIC_ASSERT(ef_scene_triplet_z, offsetof(ef_scene_triplet_packed, z) == 12);

/* 0x159094: high words mirrored into three 4-byte-spaced slots. */
typedef struct EF_PACKED ef_scene_triplet_high {
    int16_t x;
    uint8_t pad_02[2];
    int16_t y;
    uint8_t pad_06[2];
    int16_t z;
} ef_scene_triplet_high;
EF_STATIC_ASSERT(ef_scene_triplet_high_y, offsetof(ef_scene_triplet_high, y) == 4);
EF_STATIC_ASSERT(ef_scene_triplet_high_z, offsetof(ef_scene_triplet_high, z) == 8);

/* 0x2144aa: +0xac counter is simultaneously the high signed word of
 * the 32-bit word at +0xaa. It MUST NOT be modeled as independent globals. */
typedef struct EF_PACKED ef_scene_event_counter {
    uint16_t unknown_aa;
    int16_t count;                    /* +0xac */
    int16_t secondary;                /* +0xae */
} ef_scene_event_counter;
EF_STATIC_ASSERT(ef_scene_counter_high, offsetof(ef_scene_event_counter, count) == 2);
EF_STATIC_ASSERT(ef_scene_counter_secondary, offsetof(ef_scene_event_counter, secondary) == 4);

/* Position and attitude prefix used by the tracked camera. */
typedef struct EF_PACKED ef_scene_subject {
    int32_t position[3];
    uint16_t pitch;
    uint16_t yaw;
    uint16_t roll;
} ef_scene_subject;
EF_STATIC_ASSERT(ef_scene_subject_position, offsetof(ef_scene_subject, position) == 0);
EF_STATIC_ASSERT(ef_scene_subject_pitch, offsetof(ef_scene_subject, pitch) == 12);
EF_STATIC_ASSERT(ef_scene_subject_yaw, offsetof(ef_scene_subject, yaw) == 14);
EF_STATIC_ASSERT(ef_scene_subject_roll, offsetof(ef_scene_subject, roll) == 16);
EF_STATIC_ASSERT(ef_scene_subject_size, sizeof(ef_scene_subject) == 18);

#endif
