// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once
#include <cstdint>

namespace ef2000 {
// Hybrid runtime's VGA DAC write policy. Store receives a palette byte offset
// and a six-bit component. Advance only after the store completes.
class VgaDac {
public:
    template<class Store>
    bool write(uint32_t port, uint32_t value, Store store) {
        if (port == 0x3c8) { cursor_ = (value & 255u) * 3u; return true; }
        if (port != 0x3c9) return false;
        store(cursor_, uint8_t(value & 63u));
        cursor_ = (cursor_ + 1u) % 768u;
        return true;
    }
    uint32_t cursor() const { return cursor_; }
private:
    uint32_t cursor_{};
};
} // namespace ef2000
