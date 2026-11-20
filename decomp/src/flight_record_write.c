/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Name tables indexed by flight record bytes. */
#define FLIGHT_SIDE_NAMES     0x00132d34u /* by +0x00 */
#define FLIGHT_TYPE_NAMES     0x00133304u /* by +0x02 */
#define FLIGHT_SKILL_NAMES    0x0013336cu /* by +0x03 */
#define PYLON_STORE_NAMES     0x001335a0u /* by each pylon byte */
#define TARGET_AREA_NAMES     0x00132fb4u /* by +0x3a */

#define FMT_FLIGHT            0x0018e184u /* "FLIGHT %hd %s %s %s\r\n" */
#define FMT_PYLON             0x0018e19cu /* "PYLON %hd %s\r\n" */
#define FMT_WP_ROUTE          0x0018e1acu /* "WP_ROUTE %hd %hd %hd\r\n" */
#define FMT_NO_TARGET         0x0018e1c4u /* "TARGET MISSION 0 GROUP 0\r\n" */
#define FMT_TARGET_AREA       0x0018e1e0u /* "TARGET_AREA %s \"%s\"\r\n" */
#define FMT_TARGET_AREA_NULL  0x0018e1f8u /* "TARGET_AREA %s \"NULL Target area ID\"\r\n" */
#define FMT_TARGET_MISSION_A  0x0018e220u /* "TARGET MISSION %hd GROUP %hd\r\n" */
#define FMT_TARGET_MISSION_B  0x0018e240u /* the same text, a separate copy */
#define FMT_BLANK_LINE        0x0018e060u /* "\r\n" */

#define FLIGHT_PYLONS         20
#define FLIGHT_PYLON_BYTES    0x55u
#define FLIGHT_WAYPOINT_SIZE  0x19u
#define FLIGHT_AREA_ITEM_SIZE 0x15u

#define TYPE_TARGET_MISSION_A 1u
#define TYPE_TARGET_AREA_A    2u
#define TYPE_TARGET_AREA_B    10u
#define TYPE_TARGET_MISSION_B 11u

static ef_addr32 name_of(ef_addr32 table, uint32_t index)
{
    return EF_U32(table + index * 4u);
}

/* 0xb2560, 504 bytes. EAX flight record (0x7c bytes), EDX handle. Write
 * the flight as text: the FLIGHT line (count +1 and the side, type and
 * skill names), twenty PYLON lines, the coordinate record +8, the
 * WP_ROUTE line and its waypoints (+0x20, count word +0x1c), then the
 * target. Target area types (2 and 10) write a placeholder mission line,
 * the TARGET_AREA name (+0x3a) and text (+0x3b), the area coordinates
 * +0x3f, one item per flight member (+0x51) and the group list +0x74;
 * target mission types (1 and 11) write the mission and group words
 * +0x6e/+0x70 and the group list; any other type a placeholder line.
 * Every flight ends with a blank line. EDX is clobbered. Caller 0xb2b00 at 0xb2c3e. */
void write_flight_record_b2560(ef_addr32 flight, uint32_t handle)
{
    uint32_t type = EF_U8(flight + 2u);
    int16_t i;

    print_to_handle_b1bb0(handle, FMT_FLIGHT, (int32_t)EF_U8(flight + 1u),
                          name_of(FLIGHT_SIDE_NAMES, EF_U8(flight)),
                          name_of(FLIGHT_TYPE_NAMES, type),
                          name_of(FLIGHT_SKILL_NAMES, EF_U8(flight + 3u)));
    for (i = 0; i < FLIGHT_PYLONS; ++i)
        print_to_handle_b1bb0(handle, FMT_PYLON, (int32_t)i,
                              name_of(PYLON_STORE_NAMES, EF_U8(flight + FLIGHT_PYLON_BYTES + (uint32_t)i)));
    print_coordinate_record_b2a2c(flight + 8u, handle);
    print_to_handle_b1bb0(handle, FMT_WP_ROUTE, (int32_t)EF_S16(flight + 0x1cu),
                          (int32_t)EF_S16(flight + 0x1au), (int32_t)EF_S16(flight + 0x1eu));
    for (i = 0; i < EF_S16(flight + 0x1cu); ++i)
        print_move_record_b2504(EF_U32(flight + 0x20u) + (uint32_t)((int32_t)i * (int32_t)FLIGHT_WAYPOINT_SIZE),
                                i, handle);

    if (type == TYPE_TARGET_AREA_A || type == TYPE_TARGET_AREA_B) {
        print_to_handle_b1bb0(handle, FMT_NO_TARGET);
        if (EF_U32(flight + 0x3bu) != 0u)
            print_to_handle_b1bb0(handle, FMT_TARGET_AREA,
                                  name_of(TARGET_AREA_NAMES, EF_U8(flight + 0x3au)), EF_U32(flight + 0x3bu));
        else
            print_to_handle_b1bb0(handle, FMT_TARGET_AREA_NULL,
                                  name_of(TARGET_AREA_NAMES, EF_U8(flight + 0x3au)));
        print_coordinate_record_b2a2c(flight + 0x3fu, handle);
        for (i = 0; (int16_t)EF_U8(flight + 1u) > i; ++i)
            dump_record_b23bc(EF_U32(flight + 0x51u) + (uint32_t)((int32_t)i * (int32_t)FLIGHT_AREA_ITEM_SIZE),
                              i, handle, i);
        print_to_handle_b1bb0(handle, FMT_BLANK_LINE);
        dump_group_list_b29a8(EF_U32(flight + 0x74u), handle);
    } else if (type == TYPE_TARGET_MISSION_A || type == TYPE_TARGET_MISSION_B) {
        print_to_handle_b1bb0(handle, type == TYPE_TARGET_MISSION_A ? FMT_TARGET_MISSION_A : FMT_TARGET_MISSION_B,
                              (int32_t)EF_S16(flight + 0x6eu), (int32_t)EF_S16(flight + 0x70u));
        dump_group_list_b29a8(EF_U32(flight + 0x74u), handle);
    } else {
        print_to_handle_b1bb0(handle, FMT_NO_TARGET);
    }
    print_to_handle_b1bb0(handle, FMT_BLANK_LINE);
}
