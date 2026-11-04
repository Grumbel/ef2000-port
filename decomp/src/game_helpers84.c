/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Vector rotation by an angle triple and box clamping. */

/* 0x32414, 155 bytes. Rotate an integer vector by the angle triple (yaw/pitch/roll words) with a 1.15 matrix. */
void rotate_vector_by_angles_32414(ef_addr32 angles, ef_addr32 vec, ef_addr32 out)
{
    int32_t m[9];
    int32_t vx = EF_S32(vec);
    int32_t vy = EF_S32(vec + 4u);
    int32_t vz = EF_S32(vec + 8u);
    ef_addr32 mp = (ef_addr32)(uintptr_t)m;

    initialize_scene_angle_record(mp);
    update_scene_angle_first(mp, EF_U16(angles + 2u));
    update_scene_angle_second(mp, EF_U16(angles));
    update_scene_angle_third(mp, EF_U16(angles + 4u));
    EF_S32(out) = (m[1] * vy + m[0] * vx + m[2] * vz) >> 15;
    EF_S32(out + 4u) = (m[3] * vx + m[4] * vy + m[5] * vz) >> 15;
    EF_S32(out + 8u) = (m[6] * vx + m[7] * vy + m[8] * vz) >> 15;
}

/* 0xa69d0, 145 bytes. Clamp a layout box's origin and size inside the limits of its container. */
void clamp_box_to_container_a69d0(ef_addr32 container, ef_addr32 box)
{
    uint32_t limit_x;
    uint32_t limit_y;

    if ((EF_U8(box + 0x8cu) & 0x1fu) != 0)
        return;
    if (EF_S16(box) < 0)
        EF_U16(box) = 0;
    if (EF_S16(box + 2u) < 0)
        EF_U16(box + 2u) = 0;
    if (EF_U16(box + 4u) > EF_U16(container + 0x65u))
        EF_U16(box + 4u) = EF_U16(container + 0x65u);
    if (EF_U16(box + 6u) > EF_U16(container + 0x67u))
        EF_U16(box + 6u) = EF_U16(container + 0x67u);
    limit_x = EF_U16(container + 0x65u);
    if ((int32_t)EF_S16(box) + (int32_t)EF_U16(box + 4u) >= (int32_t)limit_x)
        EF_U16(box) = (uint16_t)(limit_x - EF_U16(box + 4u));
    limit_y = EF_U16(container + 0x67u);
    if ((EF_S32(box) >> 16) + (int32_t)EF_U16(box + 6u) >= (int32_t)limit_y)
        EF_U16(box + 2u) = (uint16_t)(limit_y - EF_U16(box + 6u));
}
