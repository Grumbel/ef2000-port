/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Clip rectangle; a point is inside when it is strictly between the bounds. */
#define g_clip_x_min  EF_S32(0x0017cfb0u)
#define g_clip_x_max  EF_S32(0x0017cfb4u)
#define g_clip_y_max  EF_S32(0x0017cfb8u)
#define g_clip_y_min  EF_S32(0x0017cfbcu)

/* Outcode bits of point A; point B uses the same bits shifted left by four. */
#define OUT_X_LOW   0x01u  /* x <= x_min */
#define OUT_X_HIGH  0x02u  /* x >= x_max */
#define OUT_Y_HIGH  0x04u  /* y >= y_max */
#define OUT_Y_LOW   0x08u  /* y <= y_min */

#define MAY_HIT     (-1)
#define MISSES      0

static uint32_t outcode(int32_t x, int32_t y)
{
    uint32_t code = 0;

    if (x <= g_clip_x_min)
        code |= OUT_X_LOW;
    if (x >= g_clip_x_max)
        code |= OUT_X_HIGH;
    if (y >= g_clip_y_max)
        code |= OUT_Y_HIGH;
    if (y <= g_clip_y_min)
        code |= OUT_Y_LOW;
    return code;
}

/* Which side of the line from A to B the corner (cx, cy) lies on: nonzero
   when the 64-bit cross product (cy - ay) * dx - (cx - ax) * dy is negative. */
static int corner_left(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy)
{
    int32_t dx = (int32_t)((uint32_t)bx - (uint32_t)ax);
    int32_t dy = (int32_t)((uint32_t)by - (uint32_t)ay);
    int64_t p = (int64_t)(int32_t)((uint32_t)cy - (uint32_t)ay) * dx;
    int64_t q = (int64_t)(int32_t)((uint32_t)cx - (uint32_t)ax) * dy;

    return (int64_t)((uint64_t)p - (uint64_t)q) < 0;
}

/* 0xfe730, 666 bytes. Can the segment from A = (AX, AY) to B = (BX, BY)
   touch the clip rectangle? Returns -1 when either end is inside or the
   line runs between the corners that separate the two outside regions,
   0 when the segment certainly misses. Four stack arguments, popped by
   the callee. */
int32_t segment_may_hit_rect_fe730(int32_t by, int32_t bx, int32_t ay, int32_t ax)
{
    uint32_t code = outcode(ax, ay);

    if (code == 0)
        return MAY_HIT;
    code |= outcode(bx, by) << 4;
    if (code <= 0x0fu)
        return MAY_HIT;

    switch (code) {
    /* opposite sides: the segment crosses the rectangle's band */
    case 0x12: case 0x21: case 0x48: case 0x84:
        return MAY_HIT;
    /* around one corner: the corner must lie to the left of A -> B */
    case 0x81: case 0x85: case 0xa1:
        return corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_min) ? MAY_HIT : MISSES;
    case 0x28: case 0x29: case 0x68:
        return corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_min) ? MAY_HIT : MISSES;
    case 0x42: case 0x4a: case 0x52:
        return corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_max) ? MAY_HIT : MISSES;
    case 0x14: case 0x16: case 0x94:
        return corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_max) ? MAY_HIT : MISSES;
    /* around one corner the other way: it must lie to the right */
    case 0x18: case 0x1a: case 0x58:
        return corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_min) ? MISSES : MAY_HIT;
    case 0x82: case 0x86: case 0x92:
        return corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_min) ? MISSES : MAY_HIT;
    case 0x24: case 0x25: case 0xa4:
        return corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_max) ? MISSES : MAY_HIT;
    case 0x41: case 0x49: case 0x61:
        return corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_max) ? MISSES : MAY_HIT;
    /* diagonally opposite regions: the two other corners must be on different sides */
    case 0x5a: case 0xa5:
        return corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_min)
            != corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_max) ? MAY_HIT : MISSES;
    case 0x69: case 0x96:
        return corner_left(ax, ay, bx, by, g_clip_x_max, g_clip_y_min)
            != corner_left(ax, ay, bx, by, g_clip_x_min, g_clip_y_max) ? MAY_HIT : MISSES;
    default: /* both ends beyond the same edge */
        return MISSES;
    }
}
