// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "pcm_audio.hpp"

#include <algorithm>
#include <stdexcept>

namespace ef2000 {

void PcmConverter::reset() {
    bits_ = channels_ = rate_ = generation_ = 0;
    signed_pcm_ = false;
    carry_.clear();
}

StereoSamples PcmConverter::push(const AudioBlock& block) {
    if ((block.bits != 8 && block.bits != 16) ||
        (block.channels != 1 && block.channels != 2) ||
        block.rate == 0 || block.rate > 192000)
        throw std::runtime_error("unsupported original PCM format");
    if (bits_ != block.bits || channels_ != block.channels ||
        signed_pcm_ != block.signed_pcm || rate_ != block.rate ||
        generation_ != block.generation) {
        reset();
        bits_ = block.bits;
        channels_ = block.channels;
        signed_pcm_ = block.signed_pcm;
        rate_ = block.rate;
        generation_ = block.generation;
    }
    std::vector<uint8_t> pcm = std::move(carry_);
    pcm.insert(pcm.end(), block.pcm.begin(), block.pcm.end());
    const size_t bytes_per_sample = bits_ / 8;
    const size_t frame_bytes = bytes_per_sample * channels_;
    const size_t frames = pcm.size() / frame_bytes;
    const size_t used = frames * frame_bytes;
    carry_.assign(pcm.begin() + used, pcm.end());
    StereoSamples output;
    output.rate = rate_;
    output.interleaved.resize(frames * 2);

    auto sample = [&](size_t position) -> float {
        if (bits_ == 8) {
            int value = pcm[position];
            value = signed_pcm_ ? (value < 128 ? value : value - 256) : value - 128;
            return float(value) / 128.0f;
        }
        const unsigned raw = unsigned(pcm[position]) | (unsigned(pcm[position + 1]) << 8);
        const int value = signed_pcm_ ? (raw < 32768 ? int(raw) : int(raw) - 65536)
                                      : int(raw) - 32768;
        return float(value) / 32768.0f;
    };
    for (size_t frame = 0; frame < frames; ++frame) {
        const size_t offset = frame * frame_bytes;
        for (unsigned channel = 0; channel < 2; ++channel) {
            const unsigned source_channel = channels_ == 1 ? 0 : channel;
            const float value = sample(offset + source_channel * bytes_per_sample);
            output.interleaved[frame * 2 + channel] =
                std::clamp(float(value * block.gains[channel]), -1.0f, 1.0f);
        }
    }
    return output;
}

} // namespace ef2000
