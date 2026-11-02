// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../runtime/overlay_skin.hpp"
#include <cassert>

int main() {
    const auto full = ef2000::overlay_skin_patch(0, 0, 640, 480);
    assert(full.source.x == 0 && full.source.y == 0);
    assert(full.source.w == 640 && full.source.h == 400);
    assert(full.target.w == 640 && full.target.h == 480);

    const auto moved = ef2000::overlay_skin_patch(20, 240, 250, 180);
    assert(moved.source.x == 20 && moved.source.y == 200);
    assert(moved.source.w == 250 && moved.source.h == 150);
    assert(moved.target.y == 240 && moved.target.h == 180);

    const auto clipped = ef2000::overlay_skin_patch(-30, 450, 90, 80);
    assert(clipped.target.x == 0 && clipped.target.y == 450);
    assert(clipped.target.w == 60 && clipped.target.h == 30);
    assert(clipped.source.y == 375 && clipped.source.h == 25);
    const auto outside = ef2000::overlay_skin_patch(800, 0, 20, 20);
    assert(outside.target.w == 0 && outside.target.h == 0);
}
