// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sound_blaster.hpp"

#include <cstdint>
#include <vector>

namespace ef2000 {

struct StereoSamples {
    unsigned rate{};
    std::vector<float> interleaved;
};

// Convert original SB16 DMA bytes into gain-adjusted, interleaved F32 stereo.
// SDL3 performs the remaining sample-rate conversion to the playback device.
// Keep incomplete multichannel frames across consecutive DMA blocks.
class PcmConverter {
public:
    StereoSamples push(const AudioBlock& block);
    void reset();

private:
    unsigned bits_{}, channels_{}, rate_{}, generation_{};
    bool signed_pcm_{};
    std::vector<uint8_t> carry_;
};

} // namespace ef2000
