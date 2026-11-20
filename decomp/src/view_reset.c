/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define g_gui_message_hook   EF_U32(0x00132a74u)  /* cdecl message sink */

#define STR_CAMERA_NAME      0x0018abdcu  /* "COLS_CAM1" */
#define STR_NO_VIEW_DATA     0x0018ab60u  /* "Views: No Data Found!" */
#define STR_NO_CAMERA        0x0018ac28u  /* "views: Camera Position 1 not found in SS." */

#define VIEW_MATRIX          0x0012dbecu  /* g_scene_view_matrix */
#define CAMERA_MATRIX        0x00214378u  /* g_scene_camera_matrix */

#define SHAKE_PITCH_STEP     0x100
#define SHAKE_HEADING_STEP   0x200
#define SHAKE_SETTLE_HEADING 0x2000
#define RESET_UPDATE_MODE    0x200

/* The current camera record: position and three angles. */
struct camera_pose {
    int32_t x, y, z;
    uint16_t heading, pitch, roll;
};

static void read_camera_pose(struct camera_pose *pose)
{
    read_camera_record_97ca8((ef_addr32)(uintptr_t)&pose->x, (ef_addr32)(uintptr_t)&pose->y,
                             (ef_addr32)(uintptr_t)&pose->z, (ef_addr32)(uintptr_t)&pose->heading,
                             (ef_addr32)(uintptr_t)&pose->pitch, (ef_addr32)(uintptr_t)&pose->roll);
}

/* Rebuild the camera matrix from the pose's angles. */
static void set_camera_matrix(const struct camera_pose *pose)
{
    init_bounds_d0b68(CAMERA_MATRIX);
    update_scene_angle_first(CAMERA_MATRIX, pose->pitch);
    update_scene_angle_second(CAMERA_MATRIX, pose->heading);
    update_scene_angle_third(CAMERA_MATRIX, pose->roll);
}

static void set_camera_position(const struct camera_pose *pose)
{
    g_scene_camera_position[0] = pose->x;
    g_scene_camera_position[1] = pose->y;
    g_scene_camera_position[2] = pose->z;
}

/* 0x97064, 620 bytes. Reset the view to the current camera record.
 * With a scene camera, look "COLS_CAM1" up in the view data page and take
 * its offset words and view angles (records 0 on a lookup failure, which
 * is reported); otherwise apply the remaining view shake steps, settle the
 * view when they run out, and clear the view offset and motion. Rebuild
 * the camera matrix and position, clear the update latch and the view
 * data word, and update the scene (with update mode 0x200 when no scene
 * camera is set). */
void reset_view_97064(void)
{
    struct camera_pose pose;
    ef_addr32 record = 0;
    uint32_t saved_mode;
    int32_t key;

    if (g_view_scene_camera != 0) {
        key = lookup_name_value_a1a40(STR_CAMERA_NAME);
        if (key == 0) {
            /* The original keeps its caller's ESI as the record here. */
            guest_callptr1_x(g_gui_message_hook, STR_NO_CAMERA);
        } else {
            record = find_page_pair_a1a90(g_view_data_page, key);
            if (record != 0) {
                g_view_offset[0] = EF_S16(record + 0);
                g_view_offset[1] = EF_S16(record + 2);
                g_view_offset[2] = EF_S16(record + 4);
            } else {
                guest_callptr1_x(g_gui_message_hook, STR_NO_VIEW_DATA);
            }
        }
        read_camera_pose(&pose);
        set_camera_matrix(&pose);
        init_bounds_d0b68(VIEW_MATRIX);
        update_scene_angle_first(VIEW_MATRIX, (uint16_t)-EF_U16(record + 8));
        update_scene_angle_second(VIEW_MATRIX, (uint16_t)-EF_U16(record + 6));
        set_camera_position(&pose);
        g_scene_update_latch = 0;
        g_view_data_word = 0;
        guest_fn_9357c();
        return;
    }

    read_camera_pose(&pose);
    set_camera_matrix(&pose);
    if (g_view_shake_ticks > 0) {
        update_scene_angle_first(VIEW_MATRIX, SHAKE_PITCH_STEP);
        update_scene_angle_second(VIEW_MATRIX, SHAKE_HEADING_STEP);
        if (--g_view_shake_ticks == 0) {
            init_bounds_d0b68(VIEW_MATRIX);
            update_scene_angle_second(VIEW_MATRIX, SHAKE_SETTLE_HEADING);
        }
    }
    g_view_motion[0] = 0;
    g_view_motion[1] = 0;
    g_view_motion[2] = 0;
    g_view_offset[0] = 0;
    g_view_offset[1] = 0;
    g_view_offset[2] = 0;
    g_scene_update_latch = 0;
    g_view_data_word = 0;
    set_camera_position(&pose);
    saved_mode = g_view_update_mode;
    g_view_update_mode = RESET_UPDATE_MODE;
    guest_fn_9357c();
    g_view_update_mode = saved_mode;
}
