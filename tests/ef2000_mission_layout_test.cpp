// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/ef2000_mission_layout.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace ef2000::mission;

namespace {

void test_guest_field_offsets_and_alignment() {
    static_assert(alignof(Mission) == 1);
    static_assert(alignof(Group) == 1);
    static_assert(alignof(Waypoint) == 1);
    static_assert(alignof(BaseNavigation) == 1);
    static_assert(alignof(TargetMember) == 1);
    static_assert(alignof(Pilot) == 1);
    static_assert(alignof(AttackNode) == 1);
    static_assert(alignof(AttackRoute) == 1);
    static_assert(alignof(AttackPattern) == 1);

    Group group{};
    group.unit_type = 4;
    group.unit_count = 2;
    group.flight_role = 3;
    group.status = 0;
    group.pylons[0] = 7;
    group.pylons[19] = 36;
    write_signed(group.waypoint_count_le, 5);
    write_signed(group.last_waypoint_le, 4);
    write_signed(group.position.x_le, -200);
    write_signed(group.position.y_le, 387);
    group.waypoints_guest_ptr = {0x78, 0x56, 0x34, 0x12};

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&group);
    assert(bytes[0x00] == 4);
    assert(bytes[0x01] == 2);
    assert(bytes[0x02] == 3);
    assert(bytes[0x08] == 0x38); // -200 LE
    assert(bytes[0x1c] == 5);
    assert(bytes[0x1e] == 4);
    assert(bytes[0x20] == 0x78);
    assert(bytes[0x55] == 7);
    assert(bytes[0x68] == 36);
    assert(read_signed(group.waypoint_count_le) == 5);
    assert(read_signed(group.position.x_le) == -200);
}

void test_target_attack_and_pilot_records() {
    Group group{};
    group.unit_count = 2;
    group.target_area_type = 4;
    group.target_area_name_guest_ptr = {0x11, 0x22, 0x33, 0x44};
    write_signed(group.target_area_position.z_le, 912);
    group.target_members_guest_ptr = {0x55, 0x66, 0x77, 0x88};
    group.attack_guest_ptr = {0x10, 0x32, 0x54, 0x76};
    group.pilots_guest_ptr = {0x21, 0x43, 0x65, 0x87};
    auto const* group_bytes = reinterpret_cast<std::uint8_t const*>(&group);
    assert(group_bytes[0x3a] == 4);
    assert(group_bytes[0x3b] == 0x11);
    assert(group_bytes[0x47] == 0x90); // z=912 at +0x3f + 8
    assert(group_bytes[0x51] == 0x55);
    assert(group_bytes[0x69] == 0x21);
    assert(group_bytes[0x74] == 0x10);

    TargetMember target{};
    target.target_type = 5;
    target.name_guest_ptr = {0x78, 0x56, 0x34, 0x12};
    write_signed(target.parameters_le[0], -9216);
    write_signed(target.parameters_le[5], 260);
    auto const* target_bytes = reinterpret_cast<std::uint8_t const*>(&target);
    assert(target_bytes[0x00] == 5);
    assert(target_bytes[0x01] == 0x78);
    assert(target_bytes[0x07] == 0x00); // -9216 in LE
    assert(target_bytes[0x08] == 0xdc);
    assert(target_bytes[0x11] == 0x04); // 260 in LE
    assert(target_bytes[0x12] == 0x01);

    Pilot pilot{};
    pilot.name_guest_ptr = {0x40, 0x30, 0x20, 0x10};
    pilot.parameters_05[0] = 1;
    pilot.parameters_05[4] = 5;
    write_signed(pilot.parameters_0a_le[0], -1);
    write_signed(pilot.parameters_0a_le[5], 0x1234);
    auto const* pilot_bytes = reinterpret_cast<std::uint8_t const*>(&pilot);
    assert(pilot_bytes[0] == 0x40);
    assert(pilot_bytes[4] == 0); // +4 not assigned by reader
    assert(pilot_bytes[5] == 1);
    assert(pilot_bytes[9] == 5);
    assert(pilot_bytes[0x0a] == 0xff);
    assert(pilot_bytes[0x14] == 0x34);

    AttackPattern pattern{};
    write_signed(pattern.route_count_le, 6);
    pattern.routes_guest_ptr = {0x12, 0x34, 0x56, 0x78};
    auto const* pattern_bytes = reinterpret_cast<std::uint8_t const*>(&pattern);
    assert(pattern_bytes[0] == 0);
    assert(pattern_bytes[1] == 6);
    assert(pattern_bytes[3] == 0x12);

    AttackRoute route{};
    write_signed(route.node_count_le, 4);
    route.nodes_guest_ptr = {0xaa, 0xbb, 0xcc, 0xdd};
    auto const* route_bytes = reinterpret_cast<std::uint8_t const*>(&route);
    assert(route_bytes[0] == 4);
    assert(route_bytes[2] == 0xaa);

    AttackNode node{};
    write_signed(node.parameter0_le, -1024);
    write_signed(node.parameter1_le, 2048);
    write_signed(node.parameters_08_le[0], 700);
    node.action = 0; // STRIKE; 1=STRIKE_THEN_TURN, 2=PASS
    auto const* node_bytes = reinterpret_cast<std::uint8_t const*>(&node);
    assert(node_bytes[0] == 0x00);
    assert(node_bytes[3] == 0xff);
    assert(node_bytes[4] == 0x00);
    assert(node_bytes[5] == 0x08);
    assert(node_bytes[8] == 0xbc);
    assert(node_bytes[9] == 0x02);
    assert(node_bytes[0x10] == 0);
}

void test_signed_little_endian_roundtrip() {
    Le16 low{};
    Le32 full{};
    for (const auto value : {std::int16_t{-32768}, std::int16_t{-1},
                             std::int16_t{0}, std::int16_t{32767}}) {
        write_signed(low, value);
        assert(read_signed(low) == value);
    }
    for (const auto value : {std::int32_t{-2147483647 - 1}, std::int32_t{-1},
                             std::int32_t{0}, std::int32_t{2147483647}}) {
        write_signed(full, value);
        assert(read_signed(full) == value);
    }
    write_signed(full, -200);
    assert((full == Le32{0x38, 0xff, 0xff, 0xff}));
    write_signed(low, -200);
    assert((low == Le16{0x38, 0xff}));
}

void test_original_height_conversion() {
    // 0x0000ed80 first truncates the third coordinate to signed 16 bits.
    assert(guest_waypoint_height(12000) == 120);
    assert(guest_waypoint_height(1024) == 10);
    assert(guest_waypoint_height(1001) == 10);
    assert(guest_waypoint_height(1000) == 1000);
    assert(guest_waypoint_height(-900) == -900);
    assert(guest_waypoint_height(40000) == -25536);
    assert(guest_waypoint_height(65536 + 12000) == 120);
}

void test_navigation_coordinates_clamp_only_interior() {
    std::array<Waypoint, 4> route{};
    write_signed(route[0].x_le, -100);
    write_signed(route[0].y_le, 9999);
    write_signed(route[1].x_le, 0);
    write_signed(route[1].y_le, 500);
    write_signed(route[2].x_le, 200);
    write_signed(route[2].y_le, -45);
    write_signed(route[3].x_le, 888);
    write_signed(route[3].y_le, -60);
    route[1].action = 6;
    route[1].height_le = {0x78, 0x00};
    route[1].unknown_0e = {0xff, 0xab};
    const auto before_first = route[0];
    const auto before_last = route[3];

    clamp_navigation_route(route.data(), route.size());
    assert(read_signed(route[0].x_le) == read_signed(before_first.x_le));
    assert(read_signed(route[0].y_le) == read_signed(before_first.y_le));
    assert(read_signed(route[3].x_le) == read_signed(before_last.x_le));
    assert(read_signed(route[3].y_le) == read_signed(before_last.y_le));
    assert(read_signed(route[1].x_le) == 12);
    assert(read_signed(route[1].y_le) == 387);
    assert(read_signed(route[2].x_le) == 200);
    assert(read_signed(route[2].y_le) == 12);
    assert(route[1].action == 6);
    assert(read_signed(route[1].height_le) == 120);
    assert((route[1].unknown_0e == Le16{0xff, 0xab}));
}

void test_short_routes_are_not_modified() {
    std::array<Waypoint, 2> route{};
    write_signed(route[0].x_le, -100);
    write_signed(route[1].x_le, 500);
    clamp_navigation_route(route.data(), route.size());
    assert(read_signed(route[0].x_le) == -100);
    assert(read_signed(route[1].x_le) == 500);
    clamp_navigation_route(nullptr, 5); // host-side safety guard
}

}  // namespace

int main() {
    test_guest_field_offsets_and_alignment();
    test_target_attack_and_pilot_records();
    test_signed_little_endian_roundtrip();
    test_original_height_conversion();
    test_navigation_coordinates_clamp_only_interior();
    test_short_routes_are_not_modified();
    std::cout << "mission-layout: guest sizes, nested target/attack/pilot offsets, "
                 "height and route clamps passed\n";
}
