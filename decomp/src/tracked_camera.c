/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* IMUL keeps the low 32 bits. The SAR/SHL/SBB/SAR sequence following
 * it divides that signed product by four, rounding towards zero. */
static int32_t scaled_offset(int32_t component, int32_t distance)
{
    return (int32_t)((uint32_t)component * (uint32_t)distance) / 4;
}

/* Differences wrap as signed words before the arithmetic right shift.
 * Thus an odd negative difference rounds down, unlike the distance divide. */
static uint16_t halfway_angle(uint16_t previous, uint16_t next)
{
    int32_t difference = (int16_t)(next - previous);
    return (uint16_t)(previous + (difference >> 1));
}

/* 0x94a74, 628 bytes. Follow the selected subject, preserving the original
 * call order, sticky direction-change byte, and fixed-point wraparound. */
void update_tracked_camera_94a74(void)
{
    const ef_scene_subject *subject;
    uint16_t pitch, yaw;
    int32_t distance;
    ef_addr32 view = (ef_addr32)(uintptr_t)g_scene_view_matrix;
    ef_addr32 rotation = (ef_addr32)(uintptr_t)g_scene_subject_matrix;
    ef_addr32 combined = (ef_addr32)(uintptr_t)g_scene_combined_matrix;

    g_scene_update_latch = 0;
    select_view_target_92290(0);
    if (g_scene_target == 0 || g_scene_subject == 0) {
        g_scene_change_pending = 1;
        g_scene_control_code = 12;
        return;
    }
    subject = (const ef_scene_subject *)ef_guest_memory_ptr(g_scene_subject);
    initialize_scene_angle_record(view);
    initialize_scene_angle_record(rotation);
    update_scene_angle_first(rotation, subject->yaw);
    update_scene_angle_second(rotation, subject->pitch);
    update_scene_angle_third(rotation, subject->roll);
    update_scene_angle_first(view, (uint16_t)-g_scene_direction_words[1]);
    update_scene_angle_second(view, (uint16_t)-g_scene_direction_words[0]);
    pitch = (uint16_t)angle_d13ac(view);
    yaw = (uint16_t)forward_pair_d13dc(view);
    if (pitch != 0) g_scene_direction_changed = 1;
    if (yaw != 0) g_scene_direction_changed = 1;

    distance = g_scene_distance_packed >> 16;
    g_scene_offset_x = scaled_offset(g_scene_subject_matrix[2], distance);
    g_scene_offset_y = scaled_offset(g_scene_subject_matrix[5], distance);
    g_scene_offset_z = scaled_offset(g_scene_subject_matrix[8], distance);
    matrix_multiply_d08fc(combined, rotation, view);
    g_scene_angle_saved_first = halfway_angle(g_scene_angle_saved_first,
                                             (uint16_t)angle_d13ac(combined));
    g_scene_angle_saved_second = halfway_angle(g_scene_angle_saved_second,
                                             (uint16_t)forward_pair_d13dc(combined));
    g_scene_angle_saved_third = subject->roll;
    g_scene_eye_position[1] = (int32_t)((uint32_t)g_scene_offset_x +
                                      ((uint32_t)subject->position[0] << 8));
    g_scene_eye_position[0] = (int32_t)((uint32_t)g_scene_offset_y +
                                      ((uint32_t)subject->position[1] << 8));
    g_scene_eye_position[2] = (int32_t)((uint32_t)g_scene_offset_z +
                                      ((uint32_t)subject->position[2] << 8));
    g_scene_query_pair[0] = (uint32_t)(subject->position[0] >> 8);
    g_scene_gate_limit = subject->position[1] >> 8;
    g_scene_query_pair[1] = (uint32_t)(subject->position[2] >> 8);
    request_scene_pair((int16_t)-g_scene_angle_saved_first,
                       (int16_t)-g_scene_angle_saved_third);
    set_scene_angles((int16_t)-g_scene_angle_saved_first,
                     (int16_t)-g_scene_angle_saved_second,
                     (int16_t)-g_scene_angle_saved_third);
    set_scene_input_triplet(g_scene_eye_position[1], g_scene_eye_position[0],
                           g_scene_eye_position[2]);
    scene_record_unset_940e0();
}
