/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_structs/view_keys.h"

#define SCENE_CODE_STEP_MARK 8u

static ef_key_view_preset *current_preset(void)
{
    return &g_key_view_presets[g_key_view_preset_cursor];
}

/* 0x31514, 420 bytes. Advance the scripted key view of the current
 * preset. The step's hold counts down by the tick step; while it has
 * not run out and the eye is within the step's range (any distance when
 * the range is 0) nothing changes. Otherwise the next step's track is
 * selected through 0x2fbc4 and its hold loaded; after the last step
 * (track -1) a repeating preset is selected again and any other ends the
 * script (cursor -1). In scene code 8 the step is also flagged. */
void advance_key_view_31514(void)
{
    int32_t distance = -1;
    int16_t track;

    if (current_preset()->range[g_key_view_step] != 0) {
        int32_t dz = (int32_t)abs_a28f2(g_key_view_eye[2] - g_key_view_focus[2]);
        int32_t dy = (int32_t)abs_a28f2(g_key_view_eye[1] - g_key_view_focus[1]);
        int32_t dx = (int32_t)abs_a28f2(g_key_view_eye[0] - g_key_view_focus[0]);

        distance = (int32_t)length_3d_decc4(dx, dy, dz);
    }
    g_key_view_hold = (int16_t)(g_key_view_hold - (int16_t)g_simulation_tick_step);
    if (g_key_view_hold >= 0 && distance <= current_preset()->range[g_key_view_step])
        return;
    if (g_scene_control_code == SCENE_CODE_STEP_MARK)
        g_key_view_stepped_in_scene_8 = 1u;
    ++g_key_view_step;
    track = current_preset()->track[g_key_view_step];
    if (track == -1) {
        if (current_preset()->repeat == 1u) {
            g_key_view_from_preset = 0u;
            guest_fn_2fbc4_r((ef_addr32)(uintptr_t)current_preset());
        } else {
            g_key_view_preset_cursor = -1;
        }
        return;
    }
    g_key_view_from_preset = 0u;
    guest_fn_2fbc4_r((ef_addr32)(uintptr_t)&g_key_view_tracks[track]);
    g_key_view_hold = current_preset()->hold[g_key_view_step];
}
