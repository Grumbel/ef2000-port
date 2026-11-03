// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

#include <cstdint>
#include <deque>
#include <stdexcept>

namespace ef2000 {

// Strict measured Sound Blaster 16 discovery profile. It deliberately cannot
// pretend to complete the original driver's DMA/IRQ playback self-test.
class SB16Probe {
public:
    uint8_t read(uint16_t port);
    void write(uint16_t port, uint8_t value);

protected:
    bool reset_asserted_{}, initialized_{};
    uint8_t mixer_index_{};
    uint8_t irq_status_{0x20};
    std::deque<uint8_t> data_;
};

} // namespace ef2000
