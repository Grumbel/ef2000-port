// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Recovered 32-bit guest *byte* layouts, NOT host-ABI structs. These reflect
// only writes/strides verified in the German X-32 image's mission readers.
// Unsigned byte arrays keep packed/unaligned little-endian fields portable.
// Unknown fields remain explicitly uninterpreted. No original game assets.

#include <array>
#include <cstddef>
#include <cstdint>

namespace ef2000::mission {

using Le16 = std::array<std::uint8_t, 2>;
using Le32 = std::array<std::uint8_t, 4>;

// 0x0000ee5c: three signed 32-bit coordinates, then three 16-bit parameters.
struct Position3D {
    Le32 x_le{};
    Le32 y_le{};
    Le32 z_le{};
    Le16 parameter0_le{};
    Le16 parameter1_le{};
    Le16 parameter2_le{};
};

// 0x0000ed80: 0x19-byte waypoint, also used by 0x0000fb10 for navigation.
// The text's leading WAYPOINT ordinal is skipped by the guest reader.
struct Waypoint {
    Le32 x_le{};                    // +0x00
    Le32 y_le{};                    // +0x04
    Le16 parameter0_le{};           // +0x08
    Le16 parameter1_le{};           // +0x0a
    Le16 height_le{};               // +0x0c, divided by 100 if signed >1000
    Le16 unknown_0e{};              // +0x0e, not written by the reader
    Le16 parameter2_le{};           // +0x10
    Le16 parameter3_le{};           // +0x12
    Le16 parameter4_le{};           // +0x14
    Le16 unknown_16{};              // +0x16, not written by the reader
    std::uint8_t action = 0;       // +0x18, index from 0x0000ed30
};

// 0x0000f6a8 allocates 0x1b bytes; 0x0000f604 fills these header fields.
struct Mission {
    std::array<std::uint8_t, 9> unknown_00{};
    std::uint8_t mobile_class = 0; // +0x09
    std::uint8_t mission_type = 0; // +0x0a
    std::uint8_t allegiance = 0;   // +0x0b
    std::uint8_t status = 0;       // +0x0c
    std::uint8_t unknown_0d = 0;
    Le16 parameter0_le{};           // +0x0e
    Le16 parameter1_le{};           // +0x10
    std::uint8_t group_count = 0;  // +0x12
    Le32 groups_guest_ptr{};        // +0x13, 32-bit guest pointer
    std::array<std::uint8_t, 4> unknown_17{};
};

// 0x0000eefc: one member of a TARGET_AREA (LEAD, TWO, THREE, ...).
// 0x0000f314 allocates unit_count * 0x15 and advances by 0x15 each time.
struct TargetMember {
    std::uint8_t target_type = 0;    // +0x00, target-type enum
    Le32 name_guest_ptr{};           // +0x01, separately allocated text
    std::array<std::uint8_t, 2> unknown_05{};
    std::array<Le16, 6> parameters_le{}; // +0x07, six signed numeric fields
    std::array<std::uint8_t, 2> unknown_13{};
};

// 0x0000fe4c: PIL record. Unlike the text row, the in-memory object has
// an unwritten byte at +4. 0x0000f314 uses a 0x16 stride for each pilot.
struct Pilot {
    Le32 name_guest_ptr{};           // +0x00, separately allocated text
    std::uint8_t unknown_04 = 0;
    std::array<std::uint8_t, 5> parameters_05{};
    std::array<Le16, 6> parameters_0a_le{};
};

// 0x0000efe8: ATTACK_NODE, 0x11 bytes. The action is a separate
// three-entry enum (STRIKE, STRIKE_THEN_TURN, PASS).
struct AttackNode {
    Le32 parameter0_le{};
    Le32 parameter1_le{};
    std::array<Le16, 4> parameters_08_le{};
    std::uint8_t action = 0;       // +0x10
};

// 0x0000f038: ATTACK_ROUTE, 0x06 bytes, with count * 0x11 nodes.
struct AttackRoute {
    Le16 node_count_le{};
    Le32 nodes_guest_ptr{};
};

// 0x0000f08c: ATTACK_PATTERN, 0x07 bytes, with count * 0x06 routes.
// The text's pattern name is consumed but not stored by this reader.
struct AttackPattern {
    std::uint8_t unknown_00 = 0;  // not written by 0x0000f08c
    Le16 route_count_le{};
    Le32 routes_guest_ptr{};
};

// 0x0000f604 allocates count*0x7c; 0x0000f314 parses each group.
struct Group {
    std::uint8_t unit_type = 0;    // +0x00, plane/ship/ground enum
    std::uint8_t unit_count = 0;   // +0x01
    std::uint8_t flight_role = 0;  // +0x02
    std::uint8_t status = 0;       // +0x03
    std::array<std::uint8_t, 4> unknown_04{};
    Position3D position{};          // +0x08
    Le16 route_parameter_le{};      // +0x1a, meaning unverified
    Le16 waypoint_count_le{};      // +0x1c
    Le16 last_waypoint_le{};       // +0x1e
    Le32 waypoints_guest_ptr{};     // +0x20
    std::array<std::uint8_t, 0x16> unknown_24{};
    std::uint8_t target_area_type = 0; // +0x3a, conditionally assigned
    Le32 target_area_name_guest_ptr{}; // +0x3b, conditional text allocation
    Position3D target_area_position{}; // +0x3f, conditional
    Le32 target_members_guest_ptr{};   // +0x51, conditional unit_count * 0x15
    std::array<std::uint8_t, 20> pylons{};  // +0x55, weapon enum indices
    Le32 pilots_guest_ptr{};        // +0x69, conditional
    std::array<std::uint8_t, 7> unknown_6d{};
    Le32 attack_guest_ptr{};        // +0x74, conditional
    Le32 unknown_guest_ptr{};      // +0x78
};

// 0x0000f6a8 allocates 0x53-byte BASE records; 0x0000f260 reads slots.
struct Base {
    std::array<std::uint8_t, 9> unknown_00{};
    Le16 ordinal_le{};              // +0x09
    std::uint8_t allegiance = 0;   // +0x0b
    std::uint8_t target_type = 0;  // +0x0c
    Le32 name_guest_ptr{};          // +0x0d
    Position3D position{};          // +0x11
    Le16 parameter_23_le{};         // +0x23
    std::array<std::uint8_t, 2> unknown_25{};
    std::array<std::uint8_t, 8> field_27_f64{}; // +0x27, FILD/FSTP double
    std::uint8_t field_2f = 0;
    std::array<std::uint8_t, 0x18> unknown_30{};
    Le16 timeslot_count_le{};      // +0x48
    Le32 timeslots_guest_ptr{};     // +0x4a
    std::array<std::uint8_t, 5> unknown_4e{};
};

// 0x0000ee0c writes a nine-byte timeslot entry, including packed numbers.
struct Timeslot {
    std::array<std::uint8_t, 5> parameters{};
    Le16 parameter5_le{};
    Le16 parameter7_le{};
};

// 0x0000fecc allocates 0x24 bytes; 0x0000fc54 parses a BASE_NAVIGATION.
struct BaseNavigation {
    std::array<std::uint8_t, 9> unknown_00{};
    Position3D position{};          // +0x09
    std::uint8_t field_1b = 0;     // +0x1b, overwritten by second numeric field
    Le16 field_1c_le{};
    Le16 corridor_count_le{};      // +0x1e
    Le32 corridors_guest_ptr{};     // +0x20
};

// 0x0000fc54 advances by 10 bytes, 0x0000fbcc allocates count*6 routes.
struct NavigationCorridor {
    Le16 parameter0_le{};
    Le16 parameter1_le{};
    Le16 route_count_le{};         // +0x04
    Le32 routes_guest_ptr{};       // +0x06
};

struct NavigationRoute {
    Le16 waypoint_count_le{};      // +0x00
    Le32 waypoints_guest_ptr{};     // +0x02
};

static_assert(sizeof(Position3D) == 18);
static_assert(sizeof(Waypoint) == 25);
static_assert(sizeof(Mission) == 0x1b);
static_assert(sizeof(TargetMember) == 0x15);
static_assert(sizeof(Pilot) == 0x16);
static_assert(sizeof(AttackNode) == 0x11);
static_assert(sizeof(AttackRoute) == 6);
static_assert(sizeof(AttackPattern) == 7);
static_assert(sizeof(Group) == 0x7c);
static_assert(sizeof(Base) == 0x53);
static_assert(sizeof(Timeslot) == 9);
static_assert(sizeof(BaseNavigation) == 0x24);
static_assert(sizeof(NavigationCorridor) == 10);
static_assert(sizeof(NavigationRoute) == 6);
static_assert(offsetof(Mission, group_count) == 0x12);
static_assert(offsetof(Mission, groups_guest_ptr) == 0x13);
static_assert(offsetof(Group, position) == 0x08);
static_assert(offsetof(Group, waypoint_count_le) == 0x1c);
static_assert(offsetof(Group, waypoints_guest_ptr) == 0x20);
static_assert(offsetof(Group, target_area_type) == 0x3a);
static_assert(offsetof(Group, target_area_name_guest_ptr) == 0x3b);
static_assert(offsetof(Group, target_area_position) == 0x3f);
static_assert(offsetof(Group, target_members_guest_ptr) == 0x51);
static_assert(offsetof(Group, pylons) == 0x55);
static_assert(offsetof(Group, pilots_guest_ptr) == 0x69);
static_assert(offsetof(Group, attack_guest_ptr) == 0x74);
static_assert(offsetof(Base, timeslot_count_le) == 0x48);
static_assert(offsetof(Base, ordinal_le) == 0x09);
static_assert(offsetof(Base, name_guest_ptr) == 0x0d);
static_assert(offsetof(Base, position) == 0x11);
static_assert(offsetof(Base, field_27_f64) == 0x27);
static_assert(offsetof(BaseNavigation, position) == 0x09);
static_assert(offsetof(BaseNavigation, corridor_count_le) == 0x1e);
static_assert(offsetof(NavigationCorridor, route_count_le) == 4);
static_assert(offsetof(NavigationRoute, waypoints_guest_ptr) == 2);
static_assert(offsetof(Waypoint, height_le) == 0x0c);
static_assert(offsetof(Waypoint, action) == 0x18);
static_assert(offsetof(TargetMember, name_guest_ptr) == 0x01);
static_assert(offsetof(TargetMember, parameters_le) == 0x07);
static_assert(offsetof(Pilot, parameters_05) == 0x05);
static_assert(offsetof(Pilot, parameters_0a_le) == 0x0a);
static_assert(offsetof(AttackNode, action) == 0x10);
static_assert(offsetof(AttackRoute, nodes_guest_ptr) == 0x02);
static_assert(offsetof(AttackPattern, route_count_le) == 0x01);
static_assert(offsetof(AttackPattern, routes_guest_ptr) == 0x03);

std::int16_t read_signed(Le16 const& value);
std::int32_t read_signed(Le32 const& value);
void write_signed(Le16& target, std::int16_t value);
void write_signed(Le32& target, std::int32_t value);

// Guest 0x0000ed80's 16-bit height conversion, including truncation before
// comparison. Original logic: if (height > 1000) height /= 100.
std::int16_t guest_waypoint_height(std::int32_t text_value);

// Guest 0x0000fb10: clamp x and y on *interior* route nodes only, preserving
// the first and final point (also preserves every non-coordinate byte).
void clamp_navigation_route(Waypoint* points, std::size_t count);

}  // namespace ef2000::mission
