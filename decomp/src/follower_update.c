/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_structs/follower.h"


#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#define FOLLOWER_SPEED_SCALE EF_F64(0x001833f8u)
#define UNIT_TYPE_SIZE       164u
#define UNIT_TYPE_SPEED      0x00123811u /* signed word in the high half */
#define SINE_TABLE           0x00141e06u /* signed word in the high half */
#define COSINE_TABLE         0x00142e06u

#define REC_X       0x07
#define REC_Z       0x0f
#define REC_HEADING 0x14
#define REC_TARGET  0x18 /* pointer to the int32 target position */
#define REC_NODE    0x26 /* scene node index in the high half */
#define NODE_TYPE   0x3a
#define TARGET_MODE 0x0c
#define TARGET_TIME 0x0d

#define ARRIVED_DISTANCE_SQ 0x800

static ef_follower_record *follower(int16_t index)
{
    return (ef_follower_record *)(uintptr_t)(g_external_grid_base + (uint32_t)((int32_t)index * 0x45));
}

/* 0x37eac, 309 bytes. Advance follower INDEX by VALUE. The clock in
 * g_follower_clock is first set to the reference time in seconds. Rows of
 * another grid are left alone. An active row is advanced directly
 * (0x37fe4) when IMMEDIATE is set. Otherwise a following row (state 1)
 * is updated (0x381a0) and stops following once every subitem accepts
 * it, and any other row is advanced and starts following (0x37934) once
 * any subitem accepts it. Unless IMMEDIATE is set, the alternate grid
 * mode then runs the high-level hook 0x54b9c. */
void update_follower_37eac(int16_t index, double value, uint32_t immediate)
{
    const int16_t *clock = (const int16_t *)g_reference_clock;
    ef_follower_record *record;

    g_follower_clock = (int32_t)clock[0] * 3600 + (int32_t)clock[1] * 60 + clock[2];
    record = follower(index);
    if (record->marker != g_world_grid_marker)
        return;
    if (record->active != 0u) {
        if ((uint8_t)immediate != 0u) {
            advance_follower_37fe4(index, value);
            return;
        }
        if (record->state == 1u) {
            update_follower_381a0((uint32_t)(int32_t)index);
            if (all_accept_37e60(index))
                follower(index)->state = 0u;
        } else {
            advance_follower_37fe4(index, value);
            if (any_accepts_37d3c(index)) {
                record = follower(index);
                record->state = 1u;
                guest_fn_37934((ef_addr32)(uintptr_t)record);
            }
        }
    }
    if (g_world_grid_mode != 0u && (uint8_t)immediate == 0u && follower(index)->level >= 0x1bu)
        nop_54b9c();
}

static int32_t square(int32_t value)
{
    return (int32_t)((uint32_t)value * (uint32_t)value);
}

/* 0x37fe4, 441 bytes. Move follower INDEX toward its target over the
 * period VALUE. The distance to the target is measured from the row's
 * scene node. When the node is farther than sqrt(0x800), the row steps by
 * three quarters of the node type's speed times VALUE, at least one unit,
 * and snaps to the target when the step reaches it; the row's heading word
 * is set when it steps. 0x37934 then updates the row. Once the target is
 * reached the link pointer is refreshed (0x3856c), except for a target in
 * timed mode (1) whose time lies outside [0, g_follower_clock). */
void advance_follower_37fe4(int16_t index, double value)
{
    ef_addr32 rec = (ef_addr32)(uintptr_t)follower(index);
    ef_addr32 node, target;
    int32_t tx, ty, tz, dx, dy, dz, dist_sq, speed, step;

    node = guest_lookup_object_8ffe8((int32_t)EF_U32(rec + REC_NODE) >> 16);
    if (node == 0)
        return;
    target = EF_U32(rec + REC_TARGET);
    if (target == 0)
        return;
    tx = EF_S32(target + 0);
    ty = EF_S32(target + 4);
    tz = EF_S32(target + 8);
    dx = (EF_S32(node + 0) >> 8) - tx;
    dy = (EF_S32(node + 4) >> 8) - ty;
    dz = (EF_S32(node + 8) >> 8) - tz;
    if ((int32_t)abs_a28f2(dx) < 0x8000 && (int32_t)abs_a28f2(dy) < 0x8000
        && (int32_t)abs_a28f2(dz) < 0x8000)
        dist_sq = (int32_t)((uint32_t)square(dx) + (uint32_t)square(dy)
            + (uint32_t)square(dz));
    else
        dist_sq = 0x7fffffff;

    if (dist_sq > ARRIVED_DISTANCE_SQ) {
        int32_t x = EF_S32(rec + REC_X);
        int32_t z = EF_S32(rec + REC_Z);

        speed = (int32_t)EF_U32(UNIT_TYPE_SPEED
            + (uint32_t)EF_U8(node + NODE_TYPE) * UNIT_TYPE_SIZE) >> 16;
        speed = speed * 3 / 4;
        step = (int32_t)truncate_9d72c(value * FOLLOWER_SPEED_SCALE
            * (double)speed) >> 8;
        if (step == 0)
            step = 1;
        if (square(step) < dist_sq) {
            int32_t angle = angle_of_b3b1c(tx - x, tz - z);
            uint32_t i = (uint32_t)(uint16_t)angle >> 3;
            int32_t s = (int32_t)EF_U32(SINE_TABLE + i * 2u) >> 16;
            int32_t c = (int32_t)EF_U32(COSINE_TABLE + i * 2u) >> 16;

            EF_S32(rec + REC_X) = x + ((int32_t)((uint32_t)s * (uint32_t)step) >> 15);
            EF_U16(rec + REC_HEADING) = (uint16_t)angle;
            EF_S32(rec + REC_Z) = z + ((int32_t)((uint32_t)c * (uint32_t)step) >> 15);
        } else {
            EF_S32(rec + REC_X) = tx;
            EF_S32(rec + REC_Z) = tz;
            dist_sq = 0;
        }
        guest_fn_37934(rec);
    }

    if (dist_sq <= ARRIVED_DISTANCE_SQ) {
        ef_addr32 t = EF_U32(rec + REC_TARGET);

        if (EF_U8(t + TARGET_MODE) != 1u) {
            refresh_link_pointer(rec);
        } else {
            int32_t time = EF_S32(t + TARGET_TIME);

            if (time >= 0 && time < g_follower_clock)
                refresh_link_pointer(rec);
        }
    }
}
