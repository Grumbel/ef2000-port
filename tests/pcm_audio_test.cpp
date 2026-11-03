// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/pcm_audio.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void near(float value, float expected) {
    require(std::abs(value - expected) < 0.00001f, "incorrect original PCM conversion");
}
} // namespace

int main() {
    try {
        ef2000::PcmConverter converter;
        ef2000::AudioBlock block;
        block.rate = 22050;
        block.bits = 8;
        block.channels = 1;
        block.signed_pcm = false;
        block.gains = {0.5, 1};
        block.pcm = {0, 128, 255};
        auto out = converter.push(block);
        require(out.rate == 22050 && out.interleaved.size() == 6,
                "unsigned 8-bit mono expansion");
        near(out.interleaved[0], -0.5);
        near(out.interleaved[1], -1);
        near(out.interleaved[2], 0);
        near(out.interleaved[3], 0);
        near(out.interleaved[4], 127.0f / 256);
        near(out.interleaved[5], 127.0f / 128);

        block.signed_pcm = true;
        block.pcm = {0x80, 0, 0x7f};
        out = converter.push(block);
        near(out.interleaved[0], -0.5);
        near(out.interleaved[1], -1);
        near(out.interleaved[4], 127.0f / 256);

        // 16-bit stereo frames can cross DMA buffer boundaries. The PCM
        // converter carries the incomplete frame without emitting garbage.
        block.bits = 16;
        block.channels = 2;
        block.signed_pcm = true;
        block.gains = {1, 0.25};
        block.pcm = {0, 0x80, 0xff};
        out = converter.push(block);
        require(out.interleaved.empty(), "incomplete frame must be buffered");
        block.pcm = {0x7f, 0, 0, 0, 0};
        out = converter.push(block);
        require(out.interleaved.size() == 4, "complete two split frames");
        near(out.interleaved[0], -1);
        near(out.interleaved[1], (32767.0f / 32768) * 0.25f);
        near(out.interleaved[2], 0);
        near(out.interleaved[3], 0);

        block.signed_pcm = false;
        block.channels = 1;
        block.gains = {1, 1};
        block.pcm = {0, 0, 0, 0x80, 0xff, 0xff};
        out = converter.push(block);
        require(out.interleaved.size() == 6, "unsigned 16-bit mono frames");
        near(out.interleaved[0], -1);
        near(out.interleaved[2], 0);
        near(out.interleaved[4], 32767.0f / 32768);

        block.bits = 8;
        block.channels = 2;
        block.pcm = {0, 0x40};
        out = converter.push(block);
        require(out.interleaved.size() == 2, "stereo 8-bit conversion");
        near(out.interleaved[0], -1);
        near(out.interleaved[1], -0.5);

        std::cout << "pcm-audio: signedness, gain, stereo and split-frame conversion passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pcm-audio test: " << error.what() << '\n';
        return 1;
    }
}
