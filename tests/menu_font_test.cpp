// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../runtime/menu_font.hpp"

#include <cassert>
#include <iostream>

int main() {
    ef2000::MenuFontWidths widths{};
    widths.fill(-1);
    widths['A' - ef2000::menu_font_first] = 4;
    widths['M' - ef2000::menu_font_first] = 7;
    const auto a = ef2000::menu_font_glyph(widths, 'A');
    assert(a.available && a.width == 4 && a.height == 12 && a.advance == 6);
    assert(a.x == 8 && a.y == 24); // ASCII 65 starts row 2, column 1
    const auto m = ef2000::menu_font_glyph(widths, 'M');
    assert(m.available && m.width == 7 && m.advance == 9);
    const auto space = ef2000::menu_font_glyph(widths, ' ');
    assert(space.available && space.width == 0 && space.advance == 5);
    assert(!ef2000::menu_font_glyph(widths, 'C').available);
    assert(!ef2000::menu_font_glyph(widths, 10).available);
    assert(!ef2000::menu_font_glyph(widths, 255).available);
    assert(ef2000::menu_font_atlas_width == 128);
    assert(ef2000::menu_font_atlas_height == 72);
    std::cout << "menu-font: atlas cells, proportional widths and fallback passed\n";
}
