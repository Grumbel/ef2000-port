// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "ef2000_mission_layout.hpp"

#include <algorithm>

namespace ef2000::mission {

std::int16_t read_signed(Le16 const& value) {
    std::int32_t unsigned_value = std::uint32_t(value[0]) |
                                  (std::uint32_t(value[1]) << 8);
    if (unsigned_value >= 0x8000) {
        unsigned_value -= 0x10000;
    }
    return static_cast<std::int16_t>(unsigned_value);
}

std::int32_t read_signed(Le32 const& value) {
    std::int64_t unsigned_value = std::uint64_t(value[0]) |
                                  (std::uint64_t(value[1]) << 8) |
                                  (std::uint64_t(value[2]) << 16) |
                                  (std::uint64_t(value[3]) << 24);
    if (unsigned_value >= 0x80000000LL) {
        unsigned_value -= 0x100000000LL;
    }
    return static_cast<std::int32_t>(unsigned_value);
}

void write_signed(Le16& target, std::int16_t value) {
    const std::uint32_t bits = static_cast<std::uint16_t>(value);
    target[0] = static_cast<std::uint8_t>(bits);
    target[1] = static_cast<std::uint8_t>(bits >> 8);
}

void write_signed(Le32& target, std::int32_t value) {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    for (int byte = 0; byte < 4; ++byte) {
        target[byte] = static_cast<std::uint8_t>(bits >> (byte * 8));
    }
}

std::int16_t guest_waypoint_height(std::int32_t text_value) {
    // Original uses MOV [slot],AX then reads signed WORD [slot] for the
    // threshold check. Avoid implementation-defined narrowing for overflow.
    const std::uint32_t bits = static_cast<std::uint32_t>(text_value) & 0xffff;
    const std::int32_t truncated = bits >= 0x8000 ?
                                   static_cast<std::int32_t>(bits) - 0x10000 :
                                   static_cast<std::int32_t>(bits);
    return static_cast<std::int16_t>(truncated > 1000 ? truncated / 100 : truncated);
}

void clamp_navigation_route(Waypoint* points, std::size_t count) {
    if (points == nullptr || count < 3) {
        return;
    }
    for (std::size_t i = 1; i < count - 1; ++i) {
        write_signed(points[i].x_le,
                     std::clamp(read_signed(points[i].x_le), 12, 387));
        write_signed(points[i].y_le,
                     std::clamp(read_signed(points[i].y_le), 12, 387));
    }
}

}  // namespace ef2000::mission
