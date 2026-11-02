// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ef2000 {

// Layout of the caller-extracted menu-font.bmp (no copyrighted pixels here).
constexpr int menu_font_first = 32, menu_font_last = 126;
constexpr int menu_font_cell_width = 8, menu_font_cell_height = 12;
constexpr int menu_font_columns = 16;
constexpr int menu_font_atlas_width = menu_font_cell_width * menu_font_columns;
constexpr int menu_font_atlas_height = menu_font_cell_height * 6;
using MenuFontWidths = std::array<int, menu_font_last - menu_font_first + 1>;

struct MenuFontGlyph {
    int x{}, y{}, width{}, height{}, advance{};
    bool available{};
};

inline MenuFontGlyph menu_font_glyph(const MenuFontWidths& widths, unsigned char code) {
    if (code == ' ') return {0, 0, 0, 0, 5, true};
    if (code < menu_font_first || code > menu_font_last)
        return {0, 0, 0, 0, 8, false};
    const int index = int(code) - menu_font_first;
    const int width = widths[static_cast<std::size_t>(index)];
    if (width <= 0 || width > menu_font_cell_width)
        return {0, 0, 0, 0, 8, false};
    return {index % menu_font_columns * menu_font_cell_width,
            index / menu_font_columns * menu_font_cell_height,
            width, menu_font_cell_height, width + 2, true};
}

} // namespace ef2000
