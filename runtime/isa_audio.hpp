// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace ef2000 {

// Minimal 8259 and 8237 interfaces required by the original SB16 driver.
// These are device models, independent of Unicorn and SDL.
class AudioPIC {
public:
    uint8_t read(uint16_t port) const;
    void write(uint16_t port, uint8_t value);
    void request(unsigned irq);
    std::optional<unsigned> next_irq() const;
    void begin(unsigned irq);
    void clear(unsigned irq);

private:
    std::array<uint8_t, 2> masks_{{0xf8, 0xff}};
    std::array<uint8_t, 2> pending_{};
    std::array<uint8_t, 2> servicing_{};
    std::array<bool, 2> read_isr_{};
};

class DMA8237 {
public:
    using MemoryRead = std::function<std::vector<uint8_t>(uint32_t address, size_t length)>;

    static bool handles(uint16_t port);
    uint8_t read(uint16_t port);
    void write(uint16_t port, uint8_t value);
    std::vector<uint8_t> transfer(unsigned channel, uint32_t units,
                                  const MemoryRead& read_memory);

    bool masked(unsigned channel) const { return masked_.at(channel); }
    uint16_t address(unsigned channel) const { return address_.at(channel); }
    uint16_t count(unsigned channel) const { return count_.at(channel); }

private:
    static std::optional<unsigned> page_channel(uint16_t port);

    std::array<uint16_t, 8> address_{};
    std::array<uint16_t, 8> count_{{0xffff, 0xffff, 0xffff, 0xffff,
                                   0xffff, 0xffff, 0xffff, 0xffff}};
    std::array<uint16_t, 8> base_address_{};
    std::array<uint16_t, 8> base_count_{{0xffff, 0xffff, 0xffff, 0xffff,
                                        0xffff, 0xffff, 0xffff, 0xffff}};
    std::array<uint8_t, 8> page_{};
    std::array<uint8_t, 8> mode_{};
    std::array<bool, 8> masked_{{true, true, true, true, true, true, true, true}};
    std::array<bool, 2> high_{};
};

} // namespace ef2000
