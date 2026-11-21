/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Chase-camera state around 0x12db9e and 0x214370..0x2144b6. */
#define VIEW_MODE            0x0012db9eu /* byte: current view code */
#define VIEW_DISTANCE        0x0012dba0u /* int16, zoomed in steps of 5 */
#define VIEW_OFFSET          0x0012dba2u /* int16[3]: x, y, z offset */
#define VIEW_POSITION        0x0012dba8u /* int32[3], smoothed */
#define VIEW_SAVED_MATRIX    0x0012dbc8u /* 9 dwords */
#define VIEW_SUBJECT_MATRIX  0x0012dbecu /* 9 dwords */
#define VIEW_ZOOM_LOCK       0x0012dc76u /* word */
#define VIEW_CAMERA_MATRIX   0x00214378u /* 9 dwords */
#define VIEW_PREVIOUS_MATRIX 0x0021439cu /* 9 dwords */
#define VIEW_COMBINED_MATRIX 0x00214404u /* 9 dwords */
#define VIEW_MIN_DISTANCE    0x0021448au /* int16 */
#define VIEW_SNAP_FRAMES     0x00214492u /* int16: frames left without smoothing */
#define VIEW_ZOOM_KEYS       0x002144aau /* byte: 0x10 zoom in, 0x20 zoom out */
#define VIEW_ANGLE_B0        0x002144b0u /* int16 angles, smoothed */
#define VIEW_ANGLE_B4        0x002144b4u
#define VIEW_ANGLE_B6        0x002144b6u
#define VIEW_BASE            0x0021446cu /* int32[3], 24.8 */
#define VIEW_WORLD           0x0021444cu /* int32: x (0x21444c), y (0x214448), z (0x214450) */
#define VIEW_GROUND_Y        0x00214344u /* int32 */

static void copy_dwords_9357c(ef_addr32 dst, ef_addr32 src, uint32_t count)
{
    while (count--) {
        EF_U32(dst) = EF_U32(src);
        dst += 4u;
        src += 4u;
    }
}

static int16_t view_s16(ef_addr32 address)
{
    return (int16_t)EF_U16(address);
}

static int32_t view_s32(ef_addr32 address)
{
    return (int32_t)EF_U32(address);
}

/* Matrix row ROW times the offset vector, in wrapping 32-bit arithmetic. */
static int32_t view_row_dot(uint32_t row, int16_t x, int16_t y, int16_t z)
{
    ef_addr32 m = VIEW_CAMERA_MATRIX + row * 12u;

    return (int32_t)(EF_U32(m) * (uint32_t)(int32_t)x + EF_U32(m + 4u) * (uint32_t)(int32_t)y
                     + EF_U32(m + 8u) * (uint32_t)(int32_t)z);
}

/* The distance-scaled forward axis of the combined matrix (column 2). */
static uint32_t view_forward(uint32_t row, int32_t back)
{
    return EF_U32(VIEW_COMBINED_MATRIX + row * 12u + 8u) * (uint32_t)back;
}

/* Move a smoothed angle a quarter of the way to TARGET (16-bit wrap). */
static void view_smooth_angle(ef_addr32 angle, uint32_t target)
{
    int32_t delta = (int16_t)(uint16_t)(target - EF_U16(angle));

    EF_U16(angle) = (uint16_t)(view_s16(angle) + (delta >> 2));
}

static int view_mode_in(uint8_t mode, int with_41, int with_24)
{
    return mode == 0x37u || mode == 0x38u || mode == 0x39u || mode == 0x3au
        || (with_41 && mode == 0x41u) || (with_24 && mode == 0x24u);
}

/* 0x9357c, 1293 bytes, all registers but EAX preserved: one chase-camera
 * step. Save the subject matrix, run the orientation input except in the
 * fixed views, combine the camera rotation with it and set the third angle.
 * The zoom keys move the distance by 5 down to the minimum; the camera
 * offset rotated by the camera matrix (quartered) plus the distance along
 * the combined forward axis gives the target position, taken directly while
 * snap frames remain and otherwise approached by two thirds. The view angles
 * follow likewise by a quarter. The world position is the base plus the
 * camera position; outside the fixed views, a position below the ground
 * metric is lifted onto it and the subject matrix restored when the camera
 * did not climb. Finally the scene pair, angles, angle triplet and input
 * triplet are set and byte 0x21454a raised. */
void update_chase_camera_9357c(void)
{
    int32_t x, y, z;
    int32_t target[3];
    int32_t back;
    int16_t distance;
    uint32_t i;
    uint8_t mode;

    copy_record9_d0858(VIEW_SAVED_MATRIX, VIEW_SUBJECT_MATRIX);
    copy_dwords_9357c(VIEW_COMBINED_MATRIX, VIEW_CAMERA_MATRIX, 9u);
    if (!view_mode_in(EF_U8(VIEW_MODE), 1, 1))
        apply_orientation_input_91f30();
    if (EF_U8(VIEW_MODE) == 0x24u && EF_U16(VIEW_ZOOM_LOCK) == 0u)
        apply_orientation_input_91f30();
    copy_dwords_9357c(VIEW_PREVIOUS_MATRIX, VIEW_COMBINED_MATRIX, 9u);
    matrix_multiply_d08fc(VIEW_COMBINED_MATRIX, VIEW_PREVIOUS_MATRIX, VIEW_SUBJECT_MATRIX);
    update_scene_angle_third(VIEW_COMBINED_MATRIX, EF_U16(0x0012db96u));

    if (EF_U8(VIEW_MODE) != 0x24u && (EF_U8(VIEW_ZOOM_KEYS) & 0x10u)
        && view_s16(VIEW_DISTANCE) > view_s16(VIEW_MIN_DISTANCE))
        EF_U16(VIEW_DISTANCE) = (uint16_t)(EF_U16(VIEW_DISTANCE) - 5u);
    if (!view_mode_in(EF_U8(VIEW_MODE), 1, 0) && view_s16(VIEW_DISTANCE) < view_s16(VIEW_MIN_DISTANCE))
        EF_U16(VIEW_DISTANCE) = EF_U16(VIEW_MIN_DISTANCE);
    if (EF_U8(VIEW_MODE) != 0x24u && (EF_U8(VIEW_ZOOM_KEYS) & 0x20u))
        EF_U16(VIEW_DISTANCE) = (uint16_t)(EF_U16(VIEW_DISTANCE) + 5u);

    x = view_s16(VIEW_OFFSET);
    y = view_s16(VIEW_OFFSET + 2u);
    z = view_s16(VIEW_OFFSET + 4u);
    for (i = 0; i < 3u; ++i)
        target[i] = view_row_dot(i, (int16_t)x, (int16_t)y, (int16_t)z) / 4;
    EF_U32(0x00214460u) = (uint32_t)target[0];
    EF_U32(0x002143fcu) = (uint32_t)target[1];
    EF_U32(0x0021445cu) = (uint32_t)target[2];

    distance = view_s16(VIEW_DISTANCE);
    back = -(int32_t)distance;
    if (EF_U16(VIEW_SNAP_FRAMES) != 0u) {
        for (i = 0; i < 3u; ++i)
            EF_U32(VIEW_POSITION + i * 4u) = view_forward(i, back) - (uint32_t)target[i];
    } else {
        for (i = 0; i < 3u; ++i) {
            ef_addr32 p = VIEW_POSITION + i * 4u;
            uint32_t step = view_forward(i, back) - EF_U32(p) + (uint32_t)target[i];

            EF_U32(p) += (uint32_t)((int32_t)(step * 2u) / 3);
        }
    }

    if (EF_U16(VIEW_SNAP_FRAMES) != 0u) {
        EF_U16(VIEW_ANGLE_B6) = (uint16_t)angle_d13ac(VIEW_COMBINED_MATRIX);
        EF_U16(VIEW_ANGLE_B4) = (uint16_t)forward_pair_d13dc(VIEW_COMBINED_MATRIX);
        EF_U16(VIEW_ANGLE_B0) = (uint16_t)angle_d13ec(VIEW_COMBINED_MATRIX);
        EF_U16(VIEW_SNAP_FRAMES) = (uint16_t)(EF_U16(VIEW_SNAP_FRAMES) - 1u);
    } else {
        view_smooth_angle(VIEW_ANGLE_B6, (uint32_t)angle_d13ac(VIEW_COMBINED_MATRIX));
        view_smooth_angle(VIEW_ANGLE_B4, forward_pair_d13dc(VIEW_COMBINED_MATRIX));
        view_smooth_angle(VIEW_ANGLE_B0, (uint32_t)angle_d13ec(VIEW_COMBINED_MATRIX));
    }

    EF_U32(0x0021444cu) = (uint32_t)((view_s32(VIEW_BASE) >> 8) + (view_s32(VIEW_POSITION) >> 16));
    EF_U32(0x00214448u) = (uint32_t)((view_s32(VIEW_BASE + 4u) >> 8) + (view_s32(VIEW_POSITION + 4u) >> 16));
    EF_U32(0x00214450u) = (uint32_t)((view_s32(VIEW_BASE + 8u) >> 8) + (view_s32(VIEW_POSITION + 8u) >> 16));
    EF_U32(0x00214370u) = (EF_U32(VIEW_BASE) << 8) + EF_U32(VIEW_POSITION);
    EF_U32(0x0021436cu) = (EF_U32(VIEW_BASE + 4u) << 8) + EF_U32(VIEW_POSITION + 4u);
    EF_U32(0x00214374u) = (EF_U32(VIEW_BASE + 8u) << 8) + EF_U32(VIEW_POSITION + 8u);

    mode = EF_U8(VIEW_MODE);
    if (!view_mode_in(mode, 0, 1)
        && !scene_metric_reaches_threshold(view_s32(0x0021444cu), view_s32(0x00214448u))) {
        int32_t ground = scene_metric_remaining();

        EF_U32(0x00214448u) = (uint32_t)ground;
        EF_U32(0x0021436cu) = (uint32_t)ground << 16;
        if (view_s32(VIEW_GROUND_Y) <= view_s32(VIEW_POSITION + 4u))
            copy_record9_d0858(VIEW_SUBJECT_MATRIX, VIEW_SAVED_MATRIX);
        EF_U32(VIEW_GROUND_Y) = EF_U32(VIEW_POSITION + 4u);
    }

    request_scene_pair((int16_t)-view_s16(VIEW_ANGLE_B6), (int16_t)-view_s16(VIEW_ANGLE_B0));
    set_scene_angles((int16_t)-view_s16(VIEW_ANGLE_B6), (int16_t)-view_s16(VIEW_ANGLE_B4),
                     (int16_t)-view_s16(VIEW_ANGLE_B0));
    configure_scene_angle_triplet(view_s16(VIEW_ANGLE_B6), view_s16(VIEW_ANGLE_B4), view_s16(VIEW_ANGLE_B0));
    set_scene_input_triplet(view_s32(0x00214370u), view_s32(0x0021436cu), view_s32(0x00214374u));
    EF_U8(0x0021454au) = 1;
}
