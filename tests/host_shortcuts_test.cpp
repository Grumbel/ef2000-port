// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "../runtime/host_shortcuts.hpp"

#include <cassert>
#include <iostream>

int main() {
    ef2000::HostShortcuts keys;
    using Action = ef2000::HostShortcuts::Action;
    assert(keys.key(67, false, true, false) == Action::Guest); // F10 padlock
    assert(keys.key(67, false, false, false) == Action::Guest);
    assert(keys.key(68, false, true, false) == Action::ToggleFullscreen);
    assert(keys.key(68, false, true, true) == Action::Consumed);
    assert(keys.key(68, false, false, false) == Action::Consumed);
    assert(keys.key(69, false, true, false) == Action::ToggleOverlay);
    assert(keys.key(69, false, true, true) == Action::Consumed);
    assert(keys.key(69, false, false, false) == Action::Consumed);
    assert(keys.key(40, false, true, false) == Action::Guest); // plain Enter
    assert(keys.key(40, false, false, false) == Action::Guest);
    assert(keys.key(40, true, true, false) == Action::ToggleFullscreen);
    assert(keys.key(40, true, true, true) == Action::Consumed);
    // Release must stay swallowed even if Alt was released before Enter.
    assert(keys.key(40, false, false, false) == Action::Consumed);
    assert(keys.key(40, false, true, false) == Action::Guest);
    assert(keys.key(61, false, true, false) == Action::Guest); // F4
    std::cout << "host-shortcuts: F10 passthrough, F11/F12, Alt+Enter passed\n";
}
