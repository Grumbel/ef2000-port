// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

#include "isa_audio.hpp"
#include "sb16_probe.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ef2000 {

struct AudioBlock {
    unsigned bits{}, channels{}, rate{}, generation{};
    bool signed_pcm{};
    std::array<double, 2> gains{};
    std::vector<uint8_t> pcm;
};

// Experimental original-driver SB16 model. No audio host/backend dependency:
// tests can control elapsed time and inspect the exact guest DMA bytes.
class SoundBlaster16 : public SB16Probe {
public:
    SoundBlaster16();

    static bool handles(uint16_t port);
    uint8_t read(uint16_t port);
    void write(uint16_t port, uint8_t value);

    void set_time(double now) { time_ = now; }
    void advance(double now, const DMA8237::MemoryRead& read_memory);
    std::vector<AudioBlock> take_completed();
    AudioPIC& pic() { return pic_; }
    const AudioPIC& pic() const { return pic_; }
    bool active() const { return active_.has_value(); }
    double next_deadline() const;

private:
    struct Transfer {
        unsigned bits{}, channels{}, rate{}, units{};
        bool signed_pcm{}, auto_init{}, paused{};
        double duration{}, deadline{};
    };

    void finish_command();
    AudioPIC pic_;
    DMA8237 dma_;
    std::array<uint8_t, 0x48> mixer_values_{};
    std::optional<uint8_t> command_;
    std::vector<uint8_t> arguments_;
    unsigned needed_{};
    std::optional<Transfer> active_;
    std::vector<AudioBlock> completed_;
    unsigned generation_{}, sample_rate_{22050};
    bool speaker_{};
    double time_{};
};

} // namespace ef2000
