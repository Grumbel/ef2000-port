// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

namespace ef2000 {

// SDL3 scancode values, kept in this asset-free adapter for headless tests.
// F10 (67) is intentionally never handled here: it belongs to EF2000.
class HostShortcuts {
public:
    enum class Action { Guest, Consumed, ToggleOverlay, ToggleFullscreen };

    Action key(int scan, bool alt, bool down, bool repeat) {
        constexpr int return_scan = 40, f11_scan = 68, f12_scan = 69;
        if (scan == f11_scan)
            return down && !repeat ? Action::ToggleFullscreen : Action::Consumed;
        if (scan == f12_scan)
            return down && !repeat ? Action::ToggleOverlay : Action::Consumed;
        if (scan == return_scan && (alt || fullscreen_enter_)) {
            fullscreen_enter_ = down;
            return down && !repeat ? Action::ToggleFullscreen : Action::Consumed;
        }
        return Action::Guest;
    }

private:
    bool fullscreen_enter_{};
};

} // namespace ef2000
