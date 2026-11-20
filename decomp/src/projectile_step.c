/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* Object record fields. */
#define OBJ_X               0x00u    /* int32 position, 8 fraction bits */
#define OBJ_Y               0x04u
#define OBJ_Z               0x08u
#define OBJ_HEADING         0x0cu    /* angle words for 0xcef3c */
#define OBJ_TARGET          0x12u    /* dword, cleared on impact */
#define OBJ_OWNER           0x18u    /* int16: player slot, or 0xff when unowned */
#define OBJ_FLAGS           0x24u
#define OBJ_FLAG_GHOST      0x04u    /* passes through everything */
#define OBJ_FLAG_GONE       0x08u
#define OBJ_DELAY           0x59u    /* double: positive holds the object */
#define OBJ_SPEED           0x91u    /* double */
#define OBJ_NEEDS_SPEED     0x18du   /* byte: a zero speed stops the update */

#define UNIT_NONE           0xff
#define SPEED_SCALE         0x0018a8f3u /* double 0.41010498687664 */
#define STEP_SCALE          0x0018a8fbu /* double 0.02 */

static void load_position(int32_t pos[3], ef_addr32 obj)
{
    pos[0] = EF_S32(obj + OBJ_X) >> 8;
    pos[1] = EF_S32(obj + OBJ_Y) >> 8;
    pos[2] = EF_S32(obj + OBJ_Z) >> 8;
}

static int speed_is_zero(ef_addr32 obj)
{
    return (EF_U32(obj + OBJ_SPEED + 4u) & 0x7fffffffu) == 0 && EF_U32(obj + OBJ_SPEED) == 0;
}

/* Stop the object, put it back and raise EVENT at its position. */
static void impact(int16_t id, ef_addr32 obj, int16_t event)
{
    int32_t pos[3];

    EF_U32(obj + OBJ_TARGET) = 0;
    reset_object_position_8cd78(id);
    load_position(pos, obj);
    dispatch_two_object_events((ef_addr32)(uintptr_t)pos, event);
}

/* 0x8de9c, 596 bytes. AX object id; EAX clobbered. Advance a locally
 * owned (or unowned) object that is not gone by one simulation step of
 * speed * 0.4101 * g_simulation_tick_step * 0.02 along its heading;
 * objects that need a speed and have none are left alone, and a positive
 * delay resets the object instead. The old and new positions (whole
 * units) go to g_projectile_old_ and g_projectile_new_ x/y/z and the
 * word copy g_projectile_segment. The watched unit is destroyed when it
 * reaches an object (0x90940) or a hit position (0x777f4); otherwise,
 * unless it is a ghost, an object below the ground stops (speed zeroed)
 * and raises g_projectile_ground_event, and one over a plane-map cell
 * raises g_projectile_cell_event. */
void advance_projectile_8de9c(int32_t id_in)
{
    int16_t id = (int16_t)id_in;
    ef_addr32 obj = lookup_filtered_indexed_object(id);
    int32_t pos[3];
    uint8_t hit;

    if (obj == 0)
        return;
    if (EF_U16(obj + OBJ_OWNER) != (uint16_t)g_world_grid_marker && EF_S16(obj + OBJ_OWNER) != UNIT_NONE)
        return;
    if (EF_U8(obj + OBJ_FLAGS) & OBJ_FLAG_GONE)
        return;
    if (EF_U8(obj + OBJ_NEEDS_SPEED) != 0 && speed_is_zero(obj))
        return;
    if (!(EF_F64(obj + OBJ_DELAY) <= 0.0)) {
        reset_object_position_8cd78(id);
        return;
    }

    load_position(pos, obj);
    advance_position_cef3c((uint32_t)(uintptr_t)pos, obj + OBJ_HEADING,
                           (uint32_t)(int32_t)truncate_9d72c(EF_F64(obj + OBJ_SPEED) * EF_F64(SPEED_SCALE) *
                                                             (double)(int32_t)g_simulation_tick_step *
                                                             EF_F64(STEP_SCALE)));
    g_projectile_new_x = pos[0];
    g_projectile_new_y = pos[1];
    g_projectile_new_z = pos[2];
    g_projectile_old_x = EF_S32(obj + OBJ_X) >> 8;
    g_projectile_old_y = EF_S32(obj + OBJ_Y) >> 8;
    g_projectile_old_z = EF_S32(obj + OBJ_Z) >> 8;
    g_projectile_segment[0] = (int16_t)pos[0];
    g_projectile_segment[1] = (int16_t)pos[1];
    g_projectile_segment[2] = (int16_t)pos[2];
    g_projectile_segment[3] = (int16_t)g_projectile_old_x;
    g_projectile_segment[4] = (int16_t)g_projectile_old_y;
    g_projectile_segment[5] = (int16_t)g_projectile_old_z;

    hit = 0;
    if (id == g_watched_unit) {
        hit = (uint8_t)find_object_near_90940(obj);
        if (hit == 0)
            hit = (uint8_t)check_position_hit_777f4(obj);
    }
    if (EF_U8(obj + OBJ_FLAGS) & OBJ_FLAG_GHOST)
        return;
    if (hit != 0) {
        destroy_unit_8cf10(id);
        return;
    }
    if (-(int32_t)cell_distance_at_7b16c((uint32_t)(int32_t)(int16_t)(g_scene_metric_ref_packed >> 16), pos[0],
                                         pos[2]) < pos[1]) {
        EF_U32(obj + OBJ_SPEED) = 0;
        EF_U32(obj + OBJ_SPEED + 4u) = 0;
        impact(id, obj, g_projectile_ground_event);
        return;
    }
    if (lookup_plane_cell_7ae88(0, EF_S32(obj + OBJ_X) >> 8, 0, EF_S32(obj + OBJ_Z) >> 8) != 0)
        impact(id, obj, g_projectile_cell_event);
}
