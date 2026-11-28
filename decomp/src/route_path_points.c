/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Fill a flight record's route points from an outbound path, an end point
 * and a return path. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Flight record fields. */
#define FLIGHT_KIND        0x02u /* byte */
#define FLIGHT_POINTS      0x20u /* pointer to 25-byte route points */

/* Path: int16 point count, then a pointer to 25-byte points. */
#define PATH_COUNT         0x00u
#define PATH_POINTS        0x02u

/* Route point fields. */
#define POINT_SIZE         0x19u
#define POINT_X            0x00u
#define POINT_Y            0x04u
#define POINT_W8           0x08u
#define POINT_WA           0x0au
#define POINT_SPEED        0x0cu /* word */
#define POINT_ACTION       0x18u /* byte */

static ef_addr32 point_at(ef_addr32 flight, int32_t index)
{
    return EF_U32(flight + FLIGHT_POINTS) + (uint32_t)(index * (int32_t)POINT_SIZE);
}

/* Copy one 25-byte point the way the original's rep movsd/movsb does. */
static void copy_point(ef_addr32 dst, ef_addr32 src)
{
    uint32_t k;

    for (k = 0; k < 24u; k += 4u)
        EF_U32(dst + k) = EF_U32(src + k);
    EF_U8(dst + 24u) = EF_U8(src + 24u);
}

/* Copy the points of PATH in reverse order to the points from FIRST on,
 * until index LIMIT + count; each gets action 0 and SPEED. */
static void copy_reversed(ef_addr32 flight, ef_addr32 path, int16_t first, int32_t limit, uint16_t speed)
{
    int16_t j = first;
    int16_t m = (int16_t)(EF_U16(path + PATH_COUNT) - 1u);

    while ((int32_t)j < limit + (int32_t)(int16_t)EF_U16(path + PATH_COUNT)) {
        ef_addr32 point = point_at(flight, j);

        copy_point(point, EF_U32(path + PATH_POINTS) + (uint32_t)((int32_t)m * (int32_t)POINT_SIZE));
        EF_U8(point + POINT_ACTION) = 0;
        EF_U16(point + POINT_SPEED) = speed;
        m--;
        j++;
    }
}

/* 0xb8cf8, 1057 bytes. The route starts at point 1, or at point 2 when
 * SKIP_FIRST is set for a flight kind other than 9 and RETURN_DIRECT is
 * not 1 (kind 3 always uses SKIP_FIRST). The points of OUTBOUND follow
 * with SPEED; for kinds 2 and 10 the last one gets speed 5 and action 3.
 * The next point is the end point (X, Y), whose form depends on the kind:
 * kinds 2/10 take W8/WA with action 8, kind 12 action 2, kind 3 speed 250,
 * kind 9 (or RETURN_DIRECT 1) a pair of action-10 points, kind 11 the end
 * point itself and other kinds Y - 2. Unless the kind is 12, the points of
 * RETURN_PATH follow in reverse; they start one point later after a pair
 * and are also read when RETURN_PATH is null. */
void build_route_points_b8cf8(ef_addr32 flight, ef_addr32 outbound, ef_addr32 return_path, uint32_t x,
                              uint32_t y, uint16_t w8, uint16_t wa, uint8_t skip_first, uint8_t return_direct,
                              uint16_t speed)
{
    uint32_t kind = EF_U8(flight + FLIGHT_KIND);
    int16_t start, i;
    ef_addr32 point;

    if (kind != 3u && (kind == 9u || return_direct == 1u))
        start = 1;
    else
        start = (int16_t)((skip_first != 0u) + 1);

    for (i = start; (int32_t)i < (int32_t)start + (int32_t)(int16_t)EF_U16(outbound + PATH_COUNT); i++) {
        point = point_at(flight, i);
        copy_point(point, EF_U32(outbound + PATH_POINTS) +
                              (uint32_t)(((int32_t)i - (int32_t)start) * (int32_t)POINT_SIZE));
        EF_U8(point + POINT_ACTION) = 0;
        EF_U16(point + POINT_SPEED) = speed;
        if (return_direct == 1u)
            continue;
        if ((int32_t)i == (int32_t)(int16_t)EF_U16(outbound + PATH_COUNT) + (int32_t)start - 1 &&
            (EF_U8(flight + FLIGHT_KIND) == 2u || EF_U8(flight + FLIGHT_KIND) == 10u)) {
            EF_U16(point + POINT_SPEED) = 5;
            EF_U8(point + POINT_ACTION) = 3;
        } else {
            EF_U16(point + POINT_SPEED) = speed;
            EF_U8(point + POINT_ACTION) = 0;
        }
    }

    kind = EF_U8(flight + FLIGHT_KIND);
    point = point_at(flight, i);
    if (kind == 2u || kind == 10u) {
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = y;
        EF_U16(point + POINT_W8) = w8;
        EF_U16(point + POINT_WA) = wa;
        EF_U8(point + POINT_ACTION) = 8;
    } else if (kind == 12u) {
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = y;
        EF_U16(point + POINT_W8) = 0;
        EF_U16(point + POINT_WA) = 0;
        EF_U8(point + POINT_ACTION) = 2;
        EF_U16(point + POINT_SPEED) = 0;
    } else if (kind == 3u) {
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = y;
        EF_U16(point + POINT_W8) = 0;
        EF_U16(point + POINT_WA) = 0;
        EF_U8(point + POINT_ACTION) = 0;
        EF_U16(point + POINT_SPEED) = 250;
    } else if (kind == 9u || return_direct == 1u) {
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = y;
        EF_U16(point + POINT_W8) = 0;
        EF_U16(point + POINT_WA) = 0;
        EF_U8(point + POINT_ACTION) = 10;
        EF_U16(point + POINT_SPEED) = speed;
        point = point_at(flight, (int32_t)i + 1);
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = y;
        EF_U16(point + POINT_W8) = 0;
        EF_U16(point + POINT_WA) = 0;
        EF_U8(point + POINT_ACTION) = 10;
        EF_U16(point + POINT_SPEED) = speed;
    } else {
        EF_U32(point + POINT_X) = x;
        EF_U32(point + POINT_Y) = kind == 11u ? y : y - 2u;
        EF_U16(point + POINT_W8) = 0;
        EF_U16(point + POINT_WA) = 0;
        EF_U8(point + POINT_ACTION) = 0;
        EF_U16(point + POINT_SPEED) = speed;
    }

    if (return_path != 0u && EF_U8(flight + FLIGHT_KIND) != 9u && return_direct != 1u) {
        if (EF_U8(flight + FLIGHT_KIND) == 12u)
            return;
        copy_reversed(flight, return_path, (int16_t)(i + 1), (int32_t)i + 1, speed);
    } else {
        copy_reversed(flight, return_path, (int16_t)(i + 2), (int32_t)i + 2, speed);
    }
}
