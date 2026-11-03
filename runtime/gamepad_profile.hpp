// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ef2000 {

// SDL-independent mapping for standardized gamepads. Raw joysticks retain
// their historical direct four-axis mapping and custom --joystick-axes.
struct GamepadFlightInput {
    int left_x{}, left_y{}, right_x{}, right_y{};
    std::array<bool, 4> face{}; // south, east, west, north (A/B/X/Y on Xbox)
    bool left_shoulder{}, right_shoulder{};
};

struct GamepadKeyTransition {
    int sdl_scan{};
    bool down{};
};

struct GamepadFlightOutput {
    std::array<int, 4> axes{};
    unsigned buttons{};
    std::vector<GamepadKeyTransition> keys;
};

class GamepadFlightProfile {
public:
    GamepadFlightOutput sample(const GamepadFlightInput& input, double elapsed_seconds) {
        GamepadFlightOutput out;
        const int rudder = std::abs(input.right_x) < 5000 ? 0 : input.right_x;
        const int throttle_input = std::abs(input.right_y) < 5000 ? 0 : input.right_y;
        // Thumbstick Y is a *rate* control, not a positional throttle.
        // Moving up advances thrust; releasing it leaves the setting intact.
        const double seconds = std::clamp(elapsed_seconds, 0.0, 0.1);
        throttle_ = std::clamp(throttle_ - (double(throttle_input) / 32768.0) *
                              seconds * 0.32, 0.0, 1.0);
        out.axes = {input.left_x, input.left_y,
                    int(std::lround(throttle_ * 65535.0)) - 32768, rudder};

        // The DOS gameport only has four buttons. Shoulder combinations
        // instead synthesize guest keyboard make/break events; changing
        // modifier layers releases the old key before pressing the new one.
        static constexpr std::array<int, 4> lb_keys{10, 5, 8, 21}; // G, B, E, R
        static constexpr std::array<int, 4> rb_keys{6, 40, 42, 61}; // C, Enter, Backspace, F4
        const auto& layer = input.left_shoulder ? lb_keys : rb_keys;
        const bool modified = input.left_shoulder || input.right_shoulder;
        for (unsigned i = 0; i < 4; ++i) {
            const int desired = modified && input.face[i] ? layer[i] : 0;
            if (!modified && input.face[i]) out.buttons |= 1u << i;
            if (desired == held_[i]) continue;
            if (held_[i]) out.keys.push_back({held_[i], false});
            if (desired) out.keys.push_back({desired, true});
            held_[i] = desired;
        }
        return out;
    }

    int throttle_percent() const {
        return int(std::lround(throttle_ * 100.0));
    }

private:
    double throttle_ = 0.5;
    std::array<int, 4> held_{};
};

} // namespace ef2000
