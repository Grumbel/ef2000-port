// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "../runtime/nine_slice.hpp"
#include <cassert>
#include <iostream>

int main() {
    const auto patches = ef2000::nine_slice(64, 32, 8, {10, 20, 200, 100});
    assert(patches.size() == 9);
    assert(patches[0].source.x == 0 && patches[0].source.w == 8);
    assert(patches[0].target.x == 10 && patches[0].target.w == 8);
    assert(patches[4].source.w == 48 && patches[4].source.h == 16);
    assert(patches[4].target.w == 184 && patches[4].target.h == 84);
    assert(patches[8].source.x == 56 && patches[8].source.y == 24);
    assert(patches[8].target.x == 202 && patches[8].target.y == 112);
    assert(patches[8].target.x + patches[8].target.w == 210);
    assert(patches[8].target.y + patches[8].target.h == 120);
    const auto small = ef2000::nine_slice(64, 32, 8, {0, 0, 10, 6});
    assert(small[4].target.w == 0 && small[4].target.h == 0);
    assert(small[0].target.w == 5 && small[0].target.h == 3);
    for (const auto& bad : {0, -1, 16}) {
        bool failed{};
        try { (void)ef2000::nine_slice(32, 32, float(bad), {0, 0, 20, 20}); }
        catch (const std::invalid_argument&) { failed = true; }
        assert(failed);
    }
    std::cout << "nine-slice: source corners, scaling, small targets and invalid borders passed\n";
}
