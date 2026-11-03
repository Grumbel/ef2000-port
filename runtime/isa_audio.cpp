// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "isa_audio.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ef2000 {

uint8_t AudioPIC::read(uint16_t port) const {
    if (port != 0x20 && port != 0x21 && port != 0xa0 && port != 0xa1)
        throw std::runtime_error("unsupported audio PIC read port " + std::to_string(port));
    const unsigned chip = port >= 0xa0;
    if (port & 1) return masks_[chip];
    return read_isr_[chip] ? servicing_[chip] : pending_[chip];
}

void AudioPIC::write(uint16_t port, uint8_t value) {
    if (port != 0x20 && port != 0x21 && port != 0xa0 && port != 0xa1)
        throw std::runtime_error("unsupported audio PIC write port " + std::to_string(port));
    const unsigned chip = port >= 0xa0;
    if (port & 1) {
        masks_[chip] = value;
    } else if (value == 0x0a || value == 0x0b) {
        read_isr_[chip] = value == 0x0b;
    } else if (value == 0x20) {
        const unsigned bits = servicing_[chip];
        servicing_[chip] &= ~(bits & -bits);
    } else {
        throw std::runtime_error("unsupported audio PIC command " + std::to_string(value));
    }
}

void AudioPIC::request(unsigned irq) {
    if (irq >= 16) throw std::runtime_error("audio IRQ out of range");
    pending_[irq / 8] |= uint8_t(1u << (irq % 8));
}

std::optional<unsigned> AudioPIC::next_irq() const {
    for (unsigned irq = 0; irq < 8; ++irq) {
        if ((pending_[0] & ~masks_[0] & (1u << irq)) && !servicing_[0])
            return irq;
    }
    return {};
}

void AudioPIC::begin(unsigned irq) {
    if (irq >= 8) throw std::runtime_error("only primary audio IRQs are modeled");
    const uint8_t bit = uint8_t(1u << irq);
    pending_[0] &= ~bit;
    servicing_[0] |= bit;
}

void AudioPIC::clear(unsigned irq) {
    if (irq >= 16) throw std::runtime_error("audio IRQ out of range");
    pending_[irq / 8] &= ~uint8_t(1u << (irq % 8));
}

std::optional<unsigned> DMA8237::page_channel(uint16_t port) {
    switch (port) {
        case 0x87: return 0;
        case 0x83: return 1;
        case 0x81: return 2;
        case 0x82: return 3;
        case 0x8b: return 5;
        case 0x89: return 6;
        case 0x8a: return 7;
        default: return {};
    }
}

bool DMA8237::handles(uint16_t port) {
    return page_channel(port).has_value() || port <= 7 ||
           port == 0x0a || port == 0x0b || port == 0x0c ||
           (port >= 0xc0 && port <= 0xce && !(port & 1)) ||
           port == 0xd4 || port == 0xd6 || port == 0xd8;
}

void DMA8237::write(uint16_t port, uint8_t value) {
    if (const auto channel = page_channel(port)) {
        page_[*channel] = value;
        return;
    }
    if (!handles(port)) throw std::runtime_error("unsupported DMA write port " + std::to_string(port));
    const unsigned chip = port >= 0xc0;
    const unsigned local = chip ? (port - 0xc0) / 2 : port;
    if (local == 0x0c) {
        high_[chip] = false;
    } else if (local == 0x0a) {
        masked_[chip * 4 + (value & 3)] = bool(value & 4);
    } else if (local == 0x0b) {
        mode_[chip * 4 + (value & 3)] = value;
    } else if (local < 8) {
        const unsigned channel = chip * 4 + local / 2;
        auto& target = (local & 1) ? count_ : address_;
        auto& base = (local & 1) ? base_count_ : base_address_;
        const unsigned shift = high_[chip] ? 8 : 0;
        target[channel] = uint16_t((target[channel] & (shift ? 0x00ff : 0xff00)) |
                                   (unsigned(value) << shift));
        base[channel] = target[channel];
        high_[chip] = !high_[chip];
    } else {
        throw std::runtime_error("unsupported DMA write port " + std::to_string(port));
    }
}

uint8_t DMA8237::read(uint16_t port) {
    if (const auto channel = page_channel(port)) return page_[*channel];
    if (!handles(port)) throw std::runtime_error("unsupported DMA read port " + std::to_string(port));
    const unsigned chip = port >= 0xc0;
    const unsigned local = chip ? (port - 0xc0) / 2 : port;
    if (local >= 8) throw std::runtime_error("unsupported DMA read port " + std::to_string(port));
    const unsigned channel = chip * 4 + local / 2;
    const uint16_t value = (local & 1) ? count_[channel] : address_[channel];
    const uint8_t result = uint8_t(value >> (high_[chip] ? 8 : 0));
    high_[chip] = !high_[chip];
    return result;
}

std::vector<uint8_t> DMA8237::transfer(unsigned channel, uint32_t units,
                                        const MemoryRead& read_memory) {
    if (channel >= 8) throw std::runtime_error("audio DMA channel out of range");
    if (masked_[channel]) throw std::runtime_error("audio DMA channel is masked");
    if ((mode_[channel] & 0x0c) != 8 || (mode_[channel] & 0x20))
        throw std::runtime_error("unsupported audio DMA mode " + std::to_string(mode_[channel]));
    const uint32_t scale = channel >= 4 ? 2 : 1;
    std::vector<uint8_t> result;
    result.reserve(size_t(units) * scale);
    while (units) {
        const uint32_t take = std::min({units, uint32_t(count_[channel]) + 1,
                                        0x10000u - address_[channel]});
        const uint32_t base = uint32_t(page_[channel] & (scale == 2 ? 0xfe : 0xff)) << 16;
        auto block = read_memory(base | (uint32_t(address_[channel]) * scale), take * scale);
        if (block.size() != take * scale)
            throw std::runtime_error("short audio DMA memory read");
        result.insert(result.end(), block.begin(), block.end());
        address_[channel] = uint16_t(address_[channel] + take);
        const bool terminal = take == uint32_t(count_[channel]) + 1;
        count_[channel] = uint16_t(count_[channel] - take);
        units -= take;
        if (terminal) {
            if (mode_[channel] & 0x10) {
                address_[channel] = base_address_[channel];
                count_[channel] = base_count_[channel];
            } else {
                masked_[channel] = true;
                if (units) throw std::runtime_error("audio transfer exceeds programmed DMA count");
            }
        }
    }
    return result;
}

} // namespace ef2000
