// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

namespace ef2000 {

struct SkinRect { float x{}, y{}, w{}, h{}; };
struct SkinPatch { SkinRect source, target; };

// Original menu textures were painted for 640x400 (non-square VGA pixels).
// The host overlay's 640x480 logical canvas is square-pixel 4:3. A sampled
// source rectangle must follow the destination's stretched SCREEN coordinates,
// not the window-local coordinates, even after dragging or resizing.
inline SkinPatch overlay_skin_patch(float x, float y, float width, float height) {
    const float left = std::clamp(x, 0.0f, 640.0f);
    const float top = std::clamp(y, 0.0f, 480.0f);
    const float right = std::clamp(x + width, 0.0f, 640.0f);
    const float bottom = std::clamp(y + height, 0.0f, 480.0f);
    if (right <= left || bottom <= top) return {};
    return {{left, top * (400.0f / 480.0f), right - left,
             (bottom - top) * (400.0f / 480.0f)},
            {left, top, right - left, bottom - top}};
}

} // namespace ef2000
