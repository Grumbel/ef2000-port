// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../runtime/overlay_windows.hpp"

#include <cassert>
#include <iostream>

int main() {
    using ef2000::OverlayPanel;
    ef2000::OverlayWindows windows;
    assert(!windows.visible());
    assert(!windows.button(50, 50, true, true)); // guest receives input
    windows.toggle();
    assert(windows.visible());
    assert(windows.button(600, 450, true, true)); // modal outside windows

    // Drag the controls titlebar, raise it above diagnostics, then release.
    assert(windows.button(70, 55, true, true));
    assert(windows.focused_panel() == 0 && windows.order().back() == 0);
    windows.motion(125, 105);
    windows.button(125, 105, false, true);
    assert(windows.panels()[0].x == 83 && windows.panels()[0].y == 95);
    windows.motion(500, 500);
    assert(windows.panels()[0].x == 83);

    // Resize the controls window; enforce minimum sizes.
    const auto before = windows.panels()[0];
    windows.button(before.x + before.w - 5, before.y + before.h - 5, true, true);
    windows.motion(before.x + before.w - 45, before.y + before.h - 40);
    windows.button(0, 0, false, true);
    assert(windows.panels()[0].w == before.w - 40);
    assert(windows.panels()[0].h == before.h - 35);
    const auto resized = windows.panels()[0];
    windows.button(resized.x + resized.w - 5, resized.y + resized.h - 5, true, true);
    windows.motion(-500, -500);
    windows.button(0, 0, false, true);
    assert(windows.panels()[0].w == 220);
    assert(windows.panels()[0].h == 120);

    windows.wheel(-12, 40);
    assert(windows.panels()[0].scroll == 12);
    windows.wheel(100, 40);
    assert(windows.panels()[0].scroll == 0);
    windows.focus_next();
    assert(windows.focused_panel() == 1 && windows.order().back() == 1);

    // Close both windows and reopen them on next overlay activation.
    auto p = windows.panels()[1];
    windows.button(p.x + p.w - 10, p.y + 10, true, true);
    assert(!windows.panels()[1].open);
    assert(windows.focused_panel() == 0);
    p = windows.panels()[0];
    windows.button(p.x + p.w - 10, p.y + 10, true, true);
    assert(!windows.panels()[0].open);
    windows.toggle();
    assert(!windows.visible());
    windows.toggle();
    assert(windows.panels()[0].open && windows.panels()[1].open);

    windows.open_panel(OverlayPanel::Kind::Controls);
    assert(windows.focused_panel() == 0);
    // Original titlebars also have working minimize and maximize controls.
    auto decorated = windows.panels()[0];
    windows.button(decorated.x + decorated.w - 46, decorated.y + 12, true, true);
    assert(windows.panels()[0].minimized);
    assert(windows.panels()[0].h == ef2000::OverlayWindows::title_height);
    assert(windows.button(decorated.x + 20, decorated.y + 60, true, true));
    assert(windows.panels()[0].minimized); // collapsed body is not interactive
    windows.button(decorated.x + decorated.w - 46, decorated.y + 12, true, true);
    assert(!windows.panels()[0].minimized);
    assert(windows.panels()[0].h == decorated.h);
    windows.button(decorated.x + decorated.w - 28, decorated.y + 12, true, true);
    assert(windows.panels()[0].maximized);
    assert(windows.panels()[0].w == 640 && windows.panels()[0].x == 0);
    assert(windows.panels()[0].h == 456);
    // Maximized windows cannot be accidentally dragged or resized.
    windows.button(50, 10, true, true);
    windows.motion(200, 200);
    windows.button(200, 200, false, true);
    assert(windows.panels()[0].x == 0 && windows.panels()[0].y == 0);
    windows.button(612, 12, true, true);
    assert(!windows.panels()[0].maximized);
    assert(windows.panels()[0].x == decorated.x &&
           windows.panels()[0].h == decorated.h);
    std::cout << "overlay-windows: modal input, move, resize, scroll, focus, close/reopen passed\n";
}
