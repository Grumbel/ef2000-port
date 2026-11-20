/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <string.h>

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define ROUTE_TABLE_A        0x000b4680u /* 8 dwords copied for 0xb59a8 */
#define ROUTE_TABLE_B        0x000b46a0u
#define TYPE_RECORD_SIZE     0xe5u
#define TYPE_SPEED           0x00128591u /* double per aircraft type */
#define SPEED_SCALE          0x0018e94cu /* double */
#define ROUTE_ALLOC_SIZE     0x96u
#define ROUTE_POINT_SIZE     0x19u
#define ROUTE_POINT_SLOTS    0x1f4
#define ROUTE_START_POINTS   4
#define ROUTE_ALTITUDE       0x001379a0u /* word */

/* Object: dword +0x13 is the flight record. */
#define OBJECT_FLIGHT        0x13u
/* Flight record. */
#define FLIGHT_ROUTE         0x20u
#define FLIGHT_X             0x08u
#define FLIGHT_Y             0x0cu
#define FLIGHT_TIME          0x10u
#define FLIGHT_HEADING       0x14u
#define FLIGHT_PITCH         0x16u
#define FLIGHT_POINT         0x18u
#define FLIGHT_SLOTS         0x1au
#define FLIGHT_POINTS        0x1cu
#define FLIGHT_MODE          0x1eu
#define FLIGHT_FIELD_74      0x74u
/* Route point, 0x19 bytes. */
#define POINT_X              0x00u
#define POINT_Y              0x04u
#define POINT_HEADING        0x08u
#define POINT_PITCH          0x0au
#define POINT_TIME           0x0cu
#define POINT_ALTITUDE       0x10u
#define POINT_FIELD_12       0x12u
#define POINT_FIELD_14       0x14u
#define POINT_SPEED          0x16u
#define POINT_KIND           0x18u
#define POINT_KIND_LEG       6u
#define POINT_KIND_END       7u

/* 0xb5d8c, 579 bytes. Give OBJECT's flight a fresh autopilot route from
   (X, Y) at time TIME: the route is planned by 0xb59a8 at the type's
   cruise speed, the flight starts at its first point and the four
   start points become legs, the last one ending the route. */
void build_waypoint_route_b5d8c(ef_addr32 object, int16_t x, int16_t y, int16_t time)
{
    uint32_t table_a[8];
    uint32_t table_b[8];
    ef_addr32 flight = EF_U32(object + OBJECT_FLIGHT);
    ef_addr32 route;
    ef_addr32 point;
    int32_t speed;
    int32_t speed_word;
    int16_t i;

    memcpy(table_a, ef_guest_memory_ptr(ROUTE_TABLE_A), sizeof table_a);
    memcpy(table_b, ef_guest_memory_ptr(ROUTE_TABLE_B), sizeof table_b);
    speed = (int32_t)truncate_9d72c(EF_F64(TYPE_SPEED + TYPE_RECORD_SIZE * EF_U8(flight)) * EF_F64(SPEED_SCALE));
    route = guest_fn_cc6c0_x(ROUTE_ALLOC_SIZE, 0x0018e930u); /* "Unable to alloc apm wproute" */
    EF_U32(flight + FLIGHT_ROUTE) = route;

    speed_word = (int16_t)speed;
    guest_fn_b59a8(EF_U32(object + OBJECT_FLIGHT), (int32_t)x, (int32_t)y, 0,
                   0, (uint32_t)(int32_t)time, 0, 0, (ef_addr32)(uintptr_t)table_a,
                   (ef_addr32)(uintptr_t)table_b, 4, 1, 5);

    flight = EF_U32(object + OBJECT_FLIGHT);
    route = EF_U32(flight + FLIGHT_ROUTE);
    EF_S32(route + POINT_X) = x;
    EF_S32(route + POINT_Y) = y;
    EF_U16(route + POINT_HEADING) = 0;
    EF_U16(route + POINT_PITCH) = 0;
    EF_U16(route + POINT_TIME) = (uint16_t)time;
    EF_U32(flight + FLIGHT_X) = EF_U32(route + POINT_X);
    EF_U32(flight + FLIGHT_Y) = EF_U32(route + POINT_Y);
    EF_S32(flight + FLIGHT_TIME) = time;
    EF_U16(flight + FLIGHT_HEADING) = EF_U16(route + POINT_HEADING);
    EF_U16(flight + FLIGHT_PITCH) = EF_U16(route + POINT_PITCH);
    EF_U16(flight + FLIGHT_POINT) = 0;
    EF_U16(flight + FLIGHT_SLOTS) = ROUTE_POINT_SLOTS;
    EF_U16(flight + FLIGHT_POINTS) = ROUTE_START_POINTS;
    EF_U16(flight + FLIGHT_MODE) = 1;
    EF_U32(flight + FLIGHT_FIELD_74) = 0;
    EF_U16(route + POINT_ALTITUDE) = EF_U16(ROUTE_ALTITUDE);
    EF_U16(route + POINT_FIELD_12) = 0;
    EF_U16(route + POINT_FIELD_14) = 0;

    for (i = 0; i < EF_S16(flight + FLIGHT_POINTS); i++) {
        point = EF_U32(flight + FLIGHT_ROUTE) + ROUTE_POINT_SIZE * (uint32_t)(int32_t)i;
        EF_U8(point + POINT_KIND) = POINT_KIND_LEG;
        EF_U16(point + POINT_SPEED) = (uint16_t)speed_word;
        EF_U16(point + POINT_TIME) = (uint16_t)time;
    }

    /* The last start point closes the route. */
    point = EF_U32(flight + FLIGHT_ROUTE) + ROUTE_POINT_SIZE * (uint32_t)(EF_S16(flight + FLIGHT_POINTS) - 1);
    EF_U16(point + POINT_ALTITUDE) = (uint16_t)(EF_U16(ROUTE_ALTITUDE) + 8u);
    EF_U16(point + POINT_FIELD_12) = 0;
    EF_U16(point + POINT_FIELD_14) = 0;
    EF_U8(point + POINT_KIND) = POINT_KIND_END;
}
