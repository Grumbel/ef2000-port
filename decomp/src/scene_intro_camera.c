/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define SCENE_INTRO_ENTRY_NAME 0x0018abd0u

static ef_addr32 camera_matrix(void)
{
    return (ef_addr32)(uintptr_t)g_scene_camera_matrix;
}

/* 0x9596c, 370 bytes. One frame of the intro camera set up by
 * setup_scene_camera_95ae0. While g_scene_intro_timer is positive it
 * counts down by the tick step and the camera looks at the followed
 * object (when 0x8c064 still lists it, also taking its two angle words)
 * or at the start position, with a reset camera matrix and a view matrix
 * turned by 0x1000; 0x9357c then rebuilds the view. When the timer has
 * run out, the camera matrix is rebuilt from the angles of
 * g_scene_subject and the scene entry at 0x18abd0 is looked up. Both
 * paths arm the scene event and end with a scene refresh. */
void update_scene_intro_9596c(void)
{
    int16_t timer = g_scene_intro_timer;

    if (timer <= 0) {
        g_scene_event_armed = 1;
        initialize_scene_angle_record(camera_matrix());
        update_scene_angle_first(camera_matrix(), EF_U16(g_scene_subject + 0xeu));
        update_scene_angle_second(camera_matrix(), EF_U16(g_scene_subject + 0xcu));
        update_scene_angle_third(camera_matrix(), EF_U16(g_scene_subject + 0x10u));
        lookup_entry_95660(SCENE_INTRO_ENTRY_NAME);
    } else {
        ef_addr32 object = g_scene_follow_object;
        int i;

        g_scene_intro_timer = (int16_t)(timer - (int16_t)g_simulation_tick_step);
        if (object != 0u && table_contains_8c064(object) == 1u) {
            for (i = 0; i < 3; ++i)
                g_scene_focus_position[i] = EF_S32(object + (uint32_t)i * 4u);
            g_scene_follow_angles[0] = EF_U16(object + 0xcu);
            g_scene_follow_angles[1] = EF_U16(object + 0xeu);
        } else {
            for (i = 0; i < 3; ++i)
                g_scene_focus_position[i] = g_scene_start_position[i];
        }
        for (i = 0; i < 3; ++i)
            g_scene_camera_position[i] = g_scene_focus_position[i];
        g_scene_update_latch = (uint16_t)(g_scene_update_latch + 0xfu);
        initialize_scene_angle_record(camera_matrix());
        initialize_scene_angle_record((ef_addr32)(uintptr_t)g_scene_view_matrix);
        update_scene_angle_second((ef_addr32)(uintptr_t)g_scene_view_matrix, 0x1000u);
        g_scene_event_armed = 1;
        guest_fn_9357c();
    }
    request_scene_refresh();
}
