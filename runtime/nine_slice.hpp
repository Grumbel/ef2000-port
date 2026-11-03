// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include <algorithm>
#include <array>
#include <stdexcept>

namespace ef2000 {

struct SliceRect { float x{}, y{}, w{}, h{}; };
struct SlicePatch { SliceRect source, target; };

// Texture-independent nine-slice geometry: preserve a frame's four corners,
// stretch only the four edges and the centre. The source is expected to be
// a pre-cropped frame bitmap, not an entire EF2000 background screenshot.
inline std::array<SlicePatch, 9> nine_slice(float image_width, float image_height,
                                            float border, SliceRect target) {
    if (border < 1 || image_width <= 2 * border || image_height <= 2 * border ||
        target.w <= 0 || target.h <= 0)
        throw std::invalid_argument("invalid overlay frame or slice border");
    const float bw = std::min(border, target.w / 2);
    const float bh = std::min(border, target.h / 2);
    const std::array<float, 4> sx{0, border, image_width - border, image_width};
    const std::array<float, 4> sy{0, border, image_height - border, image_height};
    const std::array<float, 4> dx{target.x, target.x + bw,
        target.x + target.w - bw, target.x + target.w};
    const std::array<float, 4> dy{target.y, target.y + bh,
        target.y + target.h - bh, target.y + target.h};
    std::array<SlicePatch, 9> patches{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            patches[row * 3 + col] = {
                {sx[col], sy[row], sx[col + 1] - sx[col], sy[row + 1] - sy[row]},
                {dx[col], dy[row], dx[col + 1] - dx[col], dy[row + 1] - dy[row]}
            };
        }
    }
    return patches;
}

} // namespace ef2000
