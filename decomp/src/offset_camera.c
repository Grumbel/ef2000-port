/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Camera that sits at a fixed offset in the subject's frame. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define CAMERA_SOURCE      0x00433f08u /* nine-dword matrix copied into the camera matrix */
#define CAMERA_SCRATCH     0x0021439cu /* copy of the camera matrix for the multiply */
#define CAMERA_FLAG        0x00214547u /* byte, cleared */
#define OFFSET_LOCAL       0x0012dba2u /* three int16: offset in the subject frame */
#define OFFSET_DISTANCE    0x0012dba0u /* int16: distance back along the view axis */
#define OFFSET_RESULT      0x0012dba8u /* three dwords: offset to the eye */
#define OFFSET_SHAKE_A     0x0012dc3cu /* three int16 added to OFFSET_SHAKE_B */
#define OFFSET_SHAKE_B     0x0012dc42u /* three int16 */
#define SNAP_BYTE          0x0012dc72u /* any of these three snaps the angles */
#define SNAP_WORD_A        0x00214492u
#define SNAP_WORD_B        0x0012dc80u

/* Signed 32-bit dot product of a matrix row with three words, wrapping. */
static uint32_t row_dot(const int32_t *row, int32_t a, int32_t b, int32_t c)
{
    return (uint32_t)row[0] * (uint32_t)a + (uint32_t)row[1] * (uint32_t)b + (uint32_t)row[2] * (uint32_t)c;
}

/* Move halfway from PREVIOUS towards NEXT; the signed word difference is
 * halved rounding towards zero. */
static uint16_t halfway_angle(uint16_t previous, int32_t next)
{
    int32_t difference = (int16_t)((int16_t)next - (int16_t)previous);

    return (uint16_t)((int32_t)(int16_t)previous + difference / 2);
}

static int16_t word_at(ef_addr32 address)
{
    return (int16_t)EF_U16(address);
}

/* 0x93a8c, 972 bytes. With a subject, load the camera matrix from
 * CAMERA_SOURCE and combine it with the view matrix, rebuild the subject's
 * rotation, and rotate OFFSET_LOCAL into the world (divided by four). The
 * saved angles follow the combined matrix, halfway per call unless one of
 * the snap flags is set. The eye sits OFFSET_DISTANCE back along the view
 * axis plus the shake offset rotated by the camera matrix (>> 15), around
 * the subject's position. */
void update_offset_camera_93a8c(void)
{
    ef_addr32 camera = (ef_addr32)(uintptr_t)g_scene_camera_matrix;
    ef_addr32 rotation = (ef_addr32)(uintptr_t)g_scene_subject_matrix;
    ef_addr32 combined = (ef_addr32)(uintptr_t)g_scene_combined_matrix;
    ef_addr32 subject = g_scene_subject;
    int32_t a, b, c, back;
    uint32_t k;

    if (subject == 0u)
        return;
    copy_record9_d0858(camera, CAMERA_SOURCE);
    for (k = 0; k < 9u; k++)
        g_scene_combined_matrix[k] = g_scene_camera_matrix[k];
    EF_U8(CAMERA_FLAG) = 0;
    for (k = 0; k < 9u; k++)
        EF_U32(CAMERA_SCRATCH + k * 4u) = (uint32_t)g_scene_combined_matrix[k];
    matrix_multiply_d08fc(combined, CAMERA_SCRATCH, (uint32_t)(uintptr_t)g_scene_view_matrix);
    initialize_scene_angle_record(rotation);
    update_scene_angle_first(rotation, EF_U16(subject + 0xeu));
    update_scene_angle_second(rotation, EF_U16(subject + 0xcu));
    update_scene_angle_third(rotation, EF_U16(subject + 0x10u));

    a = word_at(OFFSET_LOCAL);
    b = word_at(OFFSET_LOCAL + 2u);
    c = word_at(OFFSET_LOCAL + 4u);
    g_scene_offset_x = (int32_t)row_dot(&g_scene_subject_matrix[0], a, b, c) / 4;
    g_scene_offset_y = (int32_t)row_dot(&g_scene_subject_matrix[3], a, b, c) / 4;
    g_scene_offset_z = (int32_t)row_dot(&g_scene_subject_matrix[6], a, b, c) / 4;

    if (EF_U8(SNAP_BYTE) != 0u || EF_U16(SNAP_WORD_A) != 0u || EF_U16(SNAP_WORD_B) != 0u) {
        g_scene_angle_saved_first = (uint16_t)angle_d13ac(combined);
        g_scene_angle_saved_second = (uint16_t)forward_pair_d13dc(combined);
        g_scene_angle_saved_third = (uint16_t)angle_d13ec(combined);
    } else {
        g_scene_angle_saved_first = halfway_angle(g_scene_angle_saved_first, angle_d13ac(combined));
        g_scene_angle_saved_third = (uint16_t)angle_d13ec(combined);
        g_scene_angle_saved_second = halfway_angle(g_scene_angle_saved_second,
                                                   (int32_t)forward_pair_d13dc(combined));
    }

    back = -(int32_t)word_at(OFFSET_DISTANCE);
    EF_U32(OFFSET_RESULT) = (uint32_t)g_scene_offset_x + (uint32_t)g_scene_combined_matrix[2] * (uint32_t)back;
    EF_U32(OFFSET_RESULT + 4u) = (uint32_t)g_scene_offset_y + (uint32_t)g_scene_combined_matrix[5] * (uint32_t)back;
    EF_U32(OFFSET_RESULT + 8u) = (uint32_t)g_scene_offset_z + (uint32_t)g_scene_combined_matrix[8] * (uint32_t)back;
    a = (int32_t)word_at(OFFSET_SHAKE_A) + (int32_t)word_at(OFFSET_SHAKE_B);
    b = (int32_t)word_at(OFFSET_SHAKE_A + 2u) + (int32_t)word_at(OFFSET_SHAKE_B + 2u);
    c = (int32_t)word_at(OFFSET_SHAKE_A + 4u) + (int32_t)word_at(OFFSET_SHAKE_B + 4u);
    for (k = 0; k < 3u; k++)
        EF_U32(OFFSET_RESULT + k * 4u) += (uint32_t)((int32_t)row_dot(&g_scene_camera_matrix[k * 3u], a, b, c) >> 15);

    g_scene_query_pair[0] = (uint32_t)((int32_t)EF_U32(subject) >> 8);
    g_scene_gate_limit = (int32_t)EF_U32(subject + 4u) >> 8;
    g_scene_query_pair[1] = (uint32_t)((int32_t)EF_U32(subject + 8u) >> 8);
    g_scene_eye_position[1] = (int32_t)((EF_U32(subject) << 8) + EF_U32(OFFSET_RESULT));
    g_scene_eye_position[2] = (int32_t)((EF_U32(subject + 8u) << 8) + EF_U32(OFFSET_RESULT + 8u));
    g_scene_eye_position[0] = (int32_t)((EF_U32(subject + 4u) << 8) + EF_U32(OFFSET_RESULT + 4u));
    request_scene_pair((int16_t)-g_scene_angle_saved_first, (int16_t)-g_scene_angle_saved_third);
    set_scene_angles((int16_t)-g_scene_angle_saved_first, (int16_t)-g_scene_angle_saved_second,
                     (int16_t)-g_scene_angle_saved_third);
    set_scene_input_triplet(g_scene_eye_position[1], g_scene_eye_position[0], g_scene_eye_position[2]);
}
