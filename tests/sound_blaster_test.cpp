// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/sound_blaster.hpp"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool predicate, const char* message) {
    if (!predicate) throw std::runtime_error(message);
}

template<class Function> void expect_error(const char* phrase, Function function) {
    try { function(); }
    catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(phrase) != std::string::npos,
                "unexpected SB16 playback failure");
        return;
    }
    throw std::runtime_error("expected SB16 playback failure");
}

void command(ef2000::SoundBlaster16& device, std::initializer_list<uint8_t> values) {
    for (const uint8_t value : values) device.write(0x22c, value);
}
} // namespace

int main() {
    try {
        ef2000::SoundBlaster16 dsp;
        require(ef2000::SoundBlaster16::handles(0x21) &&
                ef2000::SoundBlaster16::handles(0x8b) &&
                !ef2000::SoundBlaster16::handles(0x2f8), "SB16 ISA port map");
        dsp.write(0x226, 1);
        dsp.write(0x226, 0);
        require(dsp.read(0x22a) == 0xaa, "SB16 reset handshake");
        command(dsp, {0xe1});
        require(dsp.read(0x22a) == 4 && dsp.read(0x22a) == 5, "DSP version");

        // The original driver programs channel 5 as a four-word transfer.
        for (auto [port, value] : {
                 std::pair<uint16_t, uint8_t>{0xd8, 0}, {0xc4, 0}, {0xc4, 0x80},
                 {0xc6, 3}, {0xc6, 0}, {0x8b, 0}, {0xd6, 0x49}, {0xd4, 1}})
            dsp.write(port, value);
        dsp.write(0x21, 0x78); // Permit IRQ7.
        dsp.set_time(10.0);
        command(dsp, {0x41, 0x56, 0x22, 0xb0, 0x30, 3, 0});
        const auto deadline = dsp.next_deadline();
        std::vector<uint8_t> memory(0x100000);
        for (unsigned i = 0; i < 8; ++i) memory[0x10000 + i] = uint8_t(i + 1);
        const auto read_memory = [&](uint32_t start, size_t size) {
            if (uint64_t(start) + size > memory.size())
                throw std::runtime_error("DMA reads beyond DOS RAM");
            return std::vector<uint8_t>(memory.begin() + start, memory.begin() + start + size);
        };
        dsp.advance(deadline - 0.00001, read_memory);
        require(dsp.take_completed().empty(), "premature PCM completion");
        dsp.advance(deadline, read_memory);
        const auto blocks = dsp.take_completed();
        require(blocks.size() == 1 && blocks[0].bits == 16 && blocks[0].channels == 2 &&
                blocks[0].rate == 22050 && blocks[0].signed_pcm &&
                blocks[0].pcm == std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}),
                "16-bit PCM DMA captures actual guest bytes");
        require(dsp.pic().next_irq() == 7 && !dsp.active(),
                "single-cycle DMA raises IRQ7 and stops");
        dsp.write(0x224, 0x82);
        require((dsp.read(0x225) & 3) == 2, "16-bit DSP interrupt status");
        dsp.read(0x22f); // Acknowledge 16-bit DSP completion.
        require((dsp.read(0x225) & 3) == 0, "16-bit DSP interrupt acknowledgement");
        dsp.pic().begin(7);
        dsp.write(0x20, 0x20); // Non-specific EOI.

        // Auto-init transfers continue even with an unacknowledged IRQ; the
        // original PCM blocks must survive rather than being coalesced away.
        ef2000::SoundBlaster16 looping;
        looping.write(0x226, 1);
        looping.write(0x226, 0);
        looping.read(0x22a);
        for (auto [port, value] : {
                 std::pair<uint16_t, uint8_t>{0xd8, 0}, {0xc4, 0}, {0xc4, 0x80},
                 {0xc6, 3}, {0xc6, 0}, {0x8b, 0}, {0xd6, 0x59}, {0xd4, 1}})
            looping.write(port, value);
        looping.set_time(20);
        command(looping, {0xb4, 0x30, 1, 0}); // 2 words per block, auto-init.
        const auto interval = looping.next_deadline() - 20;
        looping.advance(20 + 3.5 * interval, read_memory);
        const auto repeated = looping.take_completed();
        require(repeated.size() == 3 &&
                repeated[0].pcm == std::vector<uint8_t>({1, 2, 3, 4}) &&
                repeated[2].pcm == repeated[0].pcm, "auto-init preserves every overdue PCM block");
        looping.write(0x224, 0x82);
        require((looping.read(0x225) & 3) == 2 &&
                looping.next_deadline() > 20 + 3.5 * interval,
                "IRQ coalescing while DMA continues");
        looping.advance(20 + 3.5 * interval, read_memory);
        require(looping.take_completed().empty(), "do not duplicate completed blocks");
        looping.write(0x226, 1);
        looping.write(0x226, 0);
        require((looping.read(0x225) & 3) == 0 && !looping.active(),
                "reset clears active transfer and pending status");

        expect_error("before reset", [&] {
            ef2000::SoundBlaster16 uninitialized;
            uninitialized.write(0x22c, 0x41);
        });
        expect_error("zero SB16 sample rate", [&] { command(dsp, {0x41, 0, 0}); });
        std::cout << "sound-blaster: DSP rate, original PCM, DMA auto-init and IRQ7 passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "sound-blaster test: " << error.what() << '\n';
        return 1;
    }
}
