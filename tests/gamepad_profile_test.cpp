// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/gamepad_profile.hpp"

#include <cassert>
#include <iostream>

int main() {
    ef2000::GamepadFlightProfile profile;
    ef2000::GamepadFlightInput input;
    input.left_x = 8000;
    input.left_y = -11000;
    input.right_x = 22000;
    auto result = profile.sample(input, 0.02);
    assert(result.axes[0] == 8000 && result.axes[1] == -11000);
    assert(result.axes[3] == 22000);
    assert(result.axes[2] == 0);
    assert(profile.throttle_percent() == 50);

    input.right_y = -32768; // advance throttle using self-centering Y
    for (int n = 0; n < 100; ++n) result = profile.sample(input, 0.02);
    const int held_throttle = result.axes[2];
    assert(held_throttle > 18000);
    input.right_y = 0;
    input.right_x = 200; // dead zone
    for (int n = 0; n < 100; ++n) result = profile.sample(input, 0.02);
    assert(result.axes[2] == held_throttle);
    assert(result.axes[3] == 0);
    input.right_y = 32767;
    for (int n = 0; n < 100; ++n) result = profile.sample(input, 0.02);
    assert(result.axes[2] < held_throttle);

    input = {};
    input.face[0] = true;
    result = profile.sample(input, 0.01);
    assert(result.buttons == 1 && result.keys.empty());

    input.left_shoulder = true;
    result = profile.sample(input, 0.01);
    assert(result.buttons == 0 && result.keys.size() == 1);
    assert(result.keys[0].sdl_scan == 10 && result.keys[0].down); // G
    result = profile.sample(input, 0.01);
    assert(result.keys.empty()); // held keys must not repeat makes

    input.left_shoulder = false;
    input.right_shoulder = true;
    result = profile.sample(input, 0.01);
    assert(result.keys.size() == 2);
    assert(result.keys[0].sdl_scan == 10 && !result.keys[0].down);
    assert(result.keys[1].sdl_scan == 6 && result.keys[1].down); // C

    input = {}; // release/disconnect/focus loss
    result = profile.sample(input, 0.01);
    assert(result.keys.size() == 1 && result.keys[0].sdl_scan == 6);
    assert(!result.keys[0].down && result.buttons == 0);

    // Every shoulder chord must suppress its gameport face button. Chords
    // must release promptly when the player lifts the modifier or button.
    static constexpr int left_layer[] = {10, 5, 8, 21};
    static constexpr int right_layer[] = {6, 40, 42, 61};
    for (int layer = 0; layer < 2; ++layer) {
        for (int face = 0; face < 4; ++face) {
            input = {};
            input.face[face] = true;
            input.left_shoulder = layer == 0;
            input.right_shoulder = layer == 1;
            result = profile.sample(input, 0.01);
            assert(result.buttons == 0 && result.keys.size() == 1);
            assert(result.keys.front().down);
            assert(result.keys.front().sdl_scan ==
                   (layer == 0 ? left_layer[face] : right_layer[face]));
            result = profile.sample({}, 0.01);
            assert(result.keys.size() == 1 && !result.keys.front().down);
        }
    }

    input = {};
    input.face[2] = true;
    input.left_shoulder = input.right_shoulder = true;
    result = profile.sample(input, 0.01);
    assert(result.keys.size() == 1 && result.keys[0].sdl_scan == 8);
    assert(result.buttons == 0);
    profile.sample({}, 0.01);

    // A very large pause or frame hiccup may change the latch by at most
    // 0.1 seconds worth of rate; never integrate an unbounded delta.
    const int before_pause = profile.sample({}, 0).axes[2];
    input = {};
    input.right_y = -32768;
    const int after_pause = profile.sample(input, 10.0).axes[2];
    assert(after_pause > before_pause && after_pause - before_pause < 3000);

    std::cout << "gamepad-profile: latched throttle, rudder, LB/RB keyboard layers passed\n";
}
