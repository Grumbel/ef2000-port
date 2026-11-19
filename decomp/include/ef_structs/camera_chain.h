/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_CAMERA_CHAIN_H
#define EF_STRUCTS_CAMERA_CHAIN_H

#include "ef_types.h"

/* 0x212270: 0x4e-byte camera chain records linked by signed word index
 * from 0x12dc74 (-1 ends). Only fields read by 0x97464/0x97a94/0x97b18
 * are recovered. */
typedef struct EF_PACKED ef_camera_chain_record {
    uint8_t unknown_00[4];
    int16_t next;                    /* +0x04 */
    uint8_t unknown_06[2];
    int16_t depth;                   /* +0x08, walks stop below 2 */
    uint8_t unknown_0a[2];
    int32_t position[3];             /* +0x0c, scaled by 256 when applied */
    uint8_t unknown_18[0x0c];
    int16_t angle_a;                 /* +0x24 */
    uint8_t unknown_26[2];
    int16_t angle_b;                 /* +0x28 */
    uint8_t unknown_2a[2];
    int16_t angle_c;                 /* +0x2c */
    uint8_t unknown_2e[0x20];
} ef_camera_chain_record;
EF_STATIC_ASSERT(ef_camera_chain_next_off, offsetof(ef_camera_chain_record, next) == 4);
EF_STATIC_ASSERT(ef_camera_chain_depth_off, offsetof(ef_camera_chain_record, depth) == 8);
EF_STATIC_ASSERT(ef_camera_chain_position_off, offsetof(ef_camera_chain_record, position) == 0x0c);
EF_STATIC_ASSERT(ef_camera_chain_angle_a_off, offsetof(ef_camera_chain_record, angle_a) == 0x24);
EF_STATIC_ASSERT(ef_camera_chain_angle_b_off, offsetof(ef_camera_chain_record, angle_b) == 0x28);
EF_STATIC_ASSERT(ef_camera_chain_angle_c_off, offsetof(ef_camera_chain_record, angle_c) == 0x2c);
EF_STATIC_ASSERT(ef_camera_chain_size, sizeof(ef_camera_chain_record) == 0x4e);

#endif
