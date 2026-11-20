/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

static ef_addr32 local_addr(int32_t *p)
{
    return (ef_addr32)(uintptr_t)p;
}

static int32_t target_offset(int32_t target, uint32_t from)
{
    return (int32_t)(((uint32_t)target << 8) - from);
}

/* Point where the ray from the eye reaches height zero, along one axis. */
static int32_t ground_crossing(uint32_t from, uint32_t to, int32_t rise)
{
    return (int32_t)from + muldiv_saturate_833a0(-(int32_t)g_cursor_ray_from_y, (int32_t)(to - from), rise);
}

/* 0x834e8, 541 bytes. AX/DX cursor screen point; returns AL. Move the
 * cursor ray's ground point by the pending screen shift
 * (g_cursor_screen_shift_x/y): intersect the ray with the ground, project
 * the cursor point and the shifted point, and move the ground point by
 * the difference. Fails (0) when the ray does not descend, a point does
 * not project or the moved point is not near the eye. When the moved
 * point lies outside the target bounds (0x832b8) the ray end is set along
 * the new direction at the eye's distance to the target, otherwise to
 * the ground point itself. EDX is clobbered. */
uint8_t shift_cursor_ray_834e8(int32_t screen_x, int32_t screen_y)
{
    int16_t sx = (int16_t)screen_x;
    int16_t sy = (int16_t)screen_y;
    int32_t rise = (int32_t)(g_cursor_ray_to_y - g_cursor_ray_from_y);
    int32_t ground_x;
    int32_t ground_z;
    int32_t from_sx, from_sy, to_sx, to_sy;
    int32_t target_dist;
    int32_t ground_dist;

    if (rise <= 0)
        return 0;
    ground_x = ground_crossing(g_cursor_ray_from_x, g_cursor_ray_to_x, rise);
    ground_z = ground_crossing(g_cursor_ray_from_z, g_cursor_ray_to_z, rise);
    if (!project_point_83424(sx, sy, g_cursor_ray_from_y, local_addr(&from_sx), local_addr(&from_sy)))
        return 0;
    if (!project_point_83424((int16_t)(sx + g_cursor_screen_shift_x), (int16_t)(sy + g_cursor_screen_shift_y),
                             g_cursor_ray_from_y, local_addr(&to_sx), local_addr(&to_sy)))
        return 0;
    ground_x += from_sx - to_sx;
    ground_z += from_sy - to_sy;
    if (!ui_point_near_reference(ground_x, ground_z))
        return 0;

    if (!box_outside_bounds_832b8(ground_x, 0, ground_z)) {
        g_cursor_ray_to_z = (uint32_t)ground_z;
        g_cursor_ray_to_x = (uint32_t)ground_x;
        g_cursor_ray_to_y = 0;
        return 1;
    }
    target_dist = approx_distance3_b39ec(target_offset(g_cursor_target_x, g_cursor_ray_from_x),
                                         target_offset(g_cursor_target_y, g_cursor_ray_from_y),
                                         target_offset(g_cursor_target_z, g_cursor_ray_from_z));
    ground_dist = approx_distance3_b39ec(ground_x - (int32_t)g_cursor_ray_from_x, -(int32_t)g_cursor_ray_from_y,
                                         ground_z - (int32_t)g_cursor_ray_from_z);
    if (ground_dist <= 0)
        return 0;
    g_cursor_ray_to_x = g_cursor_ray_from_x +
                        (uint32_t)muldiv_saturate_833a0(ground_x - (int32_t)g_cursor_ray_from_x, target_dist, ground_dist);
    g_cursor_ray_to_y = g_cursor_ray_from_y +
                        (uint32_t)muldiv_saturate_833a0(-(int32_t)g_cursor_ray_from_y, target_dist, ground_dist);
    g_cursor_ray_to_z = g_cursor_ray_from_z +
                        (uint32_t)muldiv_saturate_833a0(ground_z - (int32_t)g_cursor_ray_from_z, target_dist, ground_dist);
    return 1;
}
