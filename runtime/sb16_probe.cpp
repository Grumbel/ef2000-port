// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sb16_probe.hpp"

#include <string>

namespace ef2000 {

uint8_t SB16Probe::read(uint16_t port) {
    switch (port) {
        case 0x226: return 0xff;
        case 0x22c: return 0x7f;
        case 0x22e: return data_.empty() ? 0x7f : 0xff;
        case 0x22a: {
            if (data_.empty()) return 0xff;
            const uint8_t value = data_.front();
            data_.pop_front();
            return value;
        }
        case 0x225:
            if (mixer_index_ == 0x80) return 0x04; // IRQ7
            if (mixer_index_ == 0x81) return 0x22; // DMA1 / DMA5
            if (mixer_index_ == 0x82) return 0x20; // No DSP interrupt pending
            break;
    }
    throw std::runtime_error("unsupported SB16 probe IN port " + std::to_string(port) +
                             ", mixer " + std::to_string(mixer_index_));
}

void SB16Probe::write(uint16_t port, uint8_t value) {
    if (port == 0x226) {
        if (value & 1) {
            reset_asserted_ = true;
            initialized_ = false;
            data_.clear();
        } else if (reset_asserted_) {
            reset_asserted_ = false;
            initialized_ = true;
            data_.push_back(0xaa);
        }
    } else if (port == 0x224) {
        mixer_index_ = value;
    } else if (port == 0x22c) {
        if (!initialized_) throw std::runtime_error("SB16 DSP command before reset");
        if (value == 0xe1) {
            data_.push_back(4);
            data_.push_back(5);
        } else throw std::runtime_error("unsupported SB16 DSP command " +
                                        std::to_string(value) + " (no DMA/audio yet)");
    } else throw std::runtime_error("unsupported SB16 probe OUT port " + std::to_string(port));
}

} // namespace ef2000
